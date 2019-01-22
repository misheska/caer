#include "config_server.h"

#include "caer-sdk/cross/portable_io.h"
#include "caer-sdk/cross/portable_threads.h"

#include "config_server_main.h"
#include "config_updater.h"

#include <algorithm>
#include <boost/version.hpp>
#include <string>
#include <system_error>

namespace logger = libcaer::log;
namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

static void caerConfigServerRestartListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void caerConfigServerGlobalNodeChangeListener(
	dvConfigNode node, void *userData, enum dvConfigNodeEvents event, const char *changeNode);
static void caerConfigServerGlobalAttributeChangeListener(dvConfigNode node, void *userData,
	enum dvConfigAttributeEvents event, const char *changeKey, enum dvConfigAttributeType changeType,
	union dvConfigAttributeValue changeValue);

ConfigServer::ConfigServer() :
	ioThreadRun(true),
	ioThreadState(IOThreadState::STOPPED),
	acceptor(ioService),
	acceptorNewSocket(ioService),
	sslContext(asioSSL::context::tlsv12_server),
	sslEnabled(false) {
}

void ConfigServer::threadStart() {
	ioThread = std::thread([this]() {
		// Set thread name.
		portable_thread_set_name(CONFIG_SERVER_NAME);

		while (ioThreadRun) {
			ioThreadState = IOThreadState::STARTING;

			// Ready for new run.
#if defined(BOOST_VERSION) && (BOOST_VERSION / 100000) == 1 && (BOOST_VERSION / 100 % 1000) >= 66
			ioService.restart();
#else
			ioService.reset();
#endif

			// Configure server.
			serviceConfigure();

			// Start server.
			serviceStart();

			ioThreadState = IOThreadState::STOPPED;
		}
	});
}

void ConfigServer::serviceRestart() {
	if (!ioService.stopped()) {
		ioService.post([this]() { serviceStop(); });
	}
}

void ConfigServer::threadStop() {
	if (!ioService.stopped()) {
		ioService.post([this]() {
			ioThreadRun = false;
			serviceStop();
		});
	}

	ioThread.join();
}

void ConfigServer::removeClient(ConfigServerConnection *client) {
	removePushClient(client);

	clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
}

void ConfigServer::addPushClient(ConfigServerConnection *pushClient) {
	numPushClients++;
	pushClients.push_back(pushClient);
}

void ConfigServer::removePushClient(ConfigServerConnection *pushClient) {
	pushClients.erase(std::remove(pushClients.begin(), pushClients.end(), pushClient), pushClients.end());
	numPushClients--;
}

bool ConfigServer::pushClientsPresent() {
	return (!ioService.stopped() && (numPushClients > 0));
}

void ConfigServer::pushMessageToClients(std::shared_ptr<const caerConfigActionData> message) {
	if (!ioService.stopped()) {
		ioService.post([this, message]() {
			for (auto client : pushClients) {
				client->writePushMessage(message);
			}
		});
	}
}

void ConfigServer::serviceConfigure() {
	// Get config.
	auto serverNode = dvCfg::GLOBAL.getNode("/caer/server/");

	// Configure acceptor.
	auto endpoint = asioTCP::endpoint(asioIP::address::from_string(serverNode.get<dvCfgType::STRING>("ipAddress")),
		U16T(serverNode.get<dvCfgType::INT>("portNumber")));

	acceptor.open(endpoint.protocol());
	acceptor.set_option(asioTCP::socket::reuse_address(true));
	acceptor.bind(endpoint);
	acceptor.listen();

	// Configure SSL support.
	sslEnabled = serverNode.get<dvCfgType::BOOL>("ssl");

	if (sslEnabled) {
		try {
			sslContext.use_certificate_chain_file(serverNode.get<dvCfgType::STRING>("sslCertFile"));
		}
		catch (const boost::system::system_error &ex) {
			logger::log(logger::logLevel::ERROR, CONFIG_SERVER_NAME,
				"Failed to load certificate file (error '%s'), disabling SSL.", ex.what());
			sslEnabled = false;
			return;
		}

		try {
			sslContext.use_private_key_file(
				serverNode.get<dvCfgType::STRING>("sslKeyFile"), boost::asio::ssl::context::pem);
		}
		catch (const boost::system::system_error &ex) {
			logger::log(logger::logLevel::ERROR, CONFIG_SERVER_NAME,
				"Failed to load private key file (error '%s'), disabling SSL.", ex.what());
			sslEnabled = false;
			return;
		}

		sslContext.set_options(
			boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::single_dh_use);

		// Default: no client verification enforced.
		sslContext.set_verify_mode(asioSSL::context::verify_peer);

		if (serverNode.get<dvCfgType::BOOL>("sslClientVerification")) {
			const std::string sslVerifyFile = serverNode.get<dvCfgType::STRING>("sslClientVerificationFile");

			if (sslVerifyFile.empty()) {
				sslContext.set_default_verify_paths();
			}
			else {
				try {
					sslContext.load_verify_file(sslVerifyFile);
				}
				catch (const boost::system::system_error &ex) {
					logger::log(logger::logLevel::ERROR, CONFIG_SERVER_NAME,
						"Failed to load certificate authority verification file (error '%s'), disabling SSL "
						"client verification.",
						ex.what());
					return;
				}
			}

			sslContext.set_verify_mode(asioSSL::context::verify_peer | asioSSL::context::verify_fail_if_no_peer_cert);
		}
	}
}

void ConfigServer::serviceStart() {
	logger::log(logger::logLevel::INFO, CONFIG_SERVER_NAME, "Starting configuration server service.");

	// Start accepting connections.
	acceptStart();

	ioThreadState = IOThreadState::RUNNING;

	dvCfg::GLOBAL.globalNodeListenerSet(&caerConfigServerGlobalNodeChangeListener, nullptr);
	dvCfg::GLOBAL.globalAttributeListenerSet(&caerConfigServerGlobalAttributeChangeListener, nullptr);

	// Run IO service.
	ioService.run();
}

void ConfigServer::serviceStop() {
	// Prevent multiple calls.
	if (ioThreadState != IOThreadState::RUNNING) {
		return;
	}

	ioThreadState = IOThreadState::STOPPING;

	dvCfg::GLOBAL.globalAttributeListenerSet(nullptr, nullptr);
	dvCfg::GLOBAL.globalNodeListenerSet(nullptr, nullptr);

	// Stop accepting connections.
	acceptor.close();

	// Close all open connections, hard.
	for (auto client : clients) {
		client->close();
	}

	logger::log(logger::logLevel::INFO, CONFIG_SERVER_NAME, "Stopping configuration server service.");
}

void ConfigServer::acceptStart() {
	acceptor.async_accept(acceptorNewSocket, [this](const boost::system::error_code &error) {
		if (error) {
			// Ignore cancel error, normal on shutdown.
			if (error != asio::error::operation_aborted) {
				logger::log(logger::logLevel::ERROR, CONFIG_SERVER_NAME,
					"Failed to accept new connection. Error: %s (%d).", error.message().c_str(), error.value());
			}
		}
		else {
			auto client
				= std::make_shared<ConfigServerConnection>(std::move(acceptorNewSocket), sslEnabled, &sslContext, this);

			clients.push_back(client.get());

			client->start();

			acceptStart();
		}
	});
}

static struct {
	ConfigUpdater updater;
	ConfigServer server;
} globalConfigData;

void caerConfigServerStart(void) {
	// Get the right configuration node first.
	auto serverNode = dvCfg::GLOBAL.getNode("/caer/server/");

	// Support restarting the config server.
	serverNode.create<dvCfgType::BOOL>("restart", false, {}, dvCfgFlags::NOTIFY_ONLY | dvCfgFlags::NO_EXPORT,
		"Restart configuration server, disconnects all clients and reloads itself.");

	// Ensure default values are present for IP/Port.
	serverNode.create<dvCfgType::STRING>("ipAddress", "127.0.0.1", {2, 39}, dvCfgFlags::NORMAL,
		"IP address to listen on for configuration server connections.");
	serverNode.create<dvCfgType::INT>("portNumber", 4040, {1, UINT16_MAX}, dvCfgFlags::NORMAL,
		"Port to listen on for configuration server connections.");

	// Default values for SSL encryption support.
	serverNode.create<dvCfgType::BOOL>(
		"ssl", false, {}, dvCfgFlags::NORMAL, "Require SSL encryption for configuration server communication.");
	serverNode.create<dvCfgType::STRING>(
		"sslCertFile", "", {0, PATH_MAX}, dvCfgFlags::NORMAL, "Path to SSL certificate file (PEM format).");
	serverNode.create<dvCfgType::STRING>(
		"sslKeyFile", "", {0, PATH_MAX}, dvCfgFlags::NORMAL, "Path to SSL private key file (PEM format).");

	serverNode.create<dvCfgType::BOOL>(
		"sslClientVerification", false, {}, dvCfgFlags::NORMAL, "Require SSL client certificate verification.");
	serverNode.create<dvCfgType::STRING>("sslClientVerificationFile", "", {0, PATH_MAX}, dvCfgFlags::NORMAL,
		"Path to SSL CA file for client verification (PEM format). Leave empty to use system defaults.");

	try {
		// Start threads.
		globalConfigData.server.threadStart();

		globalConfigData.updater.threadStart();
	}
	catch (const std::system_error &ex) {
		// Failed to create threads.
		logger::log(logger::logLevel::EMERGENCY, CONFIG_SERVER_NAME, "Failed to create threads. Error: %s.", ex.what());
		exit(EXIT_FAILURE);
	}

	// Listen for restart commands.
	serverNode.addAttributeListener(nullptr, &caerConfigServerRestartListener);

	// Successfully started threads.
	logger::log(logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Threads created successfully.");
}

void caerConfigServerStop(void) {
	auto serverNode = dvCfg::GLOBAL.getNode("/caer/server/");

	// Remove restart listener first.
	serverNode.removeAttributeListener(nullptr, &caerConfigServerRestartListener);

	try {
		// Stop threads.
		globalConfigData.server.threadStop();

		globalConfigData.updater.threadStop();
	}
	catch (const std::system_error &ex) {
		// Failed to join threads.
		logger::log(
			logger::logLevel::EMERGENCY, CONFIG_SERVER_NAME, "Failed to terminate threads. Error: %s.", ex.what());
		exit(EXIT_FAILURE);
	}

	// Successfully joined threads.
	logger::log(logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Threads terminated successfully.");
}

static void caerConfigServerRestartListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);
	UNUSED_ARGUMENT(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "restart")
		&& changeValue.boolean) {
		globalConfigData.server.serviceRestart();
	}
}

static void caerConfigServerGlobalNodeChangeListener(
	dvConfigNode n, void *userData, enum dvConfigNodeEvents event, const char *changeNode) {
	UNUSED_ARGUMENT(userData);
	dvCfg::Node node(n);

	if (globalConfigData.server.pushClientsPresent()) {
		auto msg = std::make_shared<caerConfigActionData>();

		msg->setAction(caerConfigAction::PUSH_MESSAGE_NODE);

		msg->setExtra(std::string(1, (char) event));

		std::string nodePath(node.getPath());
		nodePath += changeNode;
		nodePath.push_back('/');
		msg->setNode(nodePath);

		globalConfigData.server.pushMessageToClients(msg);
	}
}

static void caerConfigServerGlobalAttributeChangeListener(dvConfigNode n, void *userData,
	enum dvConfigAttributeEvents event, const char *changeKey, enum dvConfigAttributeType changeType,
	union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(userData);
	dvCfg::Node node(n);
	dvCfg::AttributeType type = static_cast<dvCfg::AttributeType>(changeType);

	if (globalConfigData.server.pushClientsPresent()) {
		auto msg = std::make_shared<caerConfigActionData>();

		msg->setAction(caerConfigAction::PUSH_MESSAGE_ATTR);
		msg->setType(type);

		if (event == DVCFG_ATTRIBUTE_ADDED) {
			// Need to get extra info when adding: flags, range, description.
			const std::string flagsStr = dvCfg::Helper::flagsToStringConverter(node.getAttributeFlags(changeKey, type));

			const std::string rangesStr
				= dvCfg::Helper::rangesToStringConverter(type, node.getAttributeRanges(changeKey, type));

			const std::string descriptionStr = node.getAttributeDescription(changeKey, type);

			std::string extraStr(1, (char) event);
			extraStr += flagsStr;
			extraStr.push_back('\0');
			extraStr += rangesStr;
			extraStr.push_back('\0');
			extraStr += descriptionStr;

			msg->setExtra(extraStr);
		}
		else {
			msg->setExtra(std::string(1, (char) event));
		}

		msg->setNode(node.getPath());
		msg->setKey(changeKey);

		const std::string valueStr = dvCfg::Helper::valueToStringConverter(type, changeValue);

		msg->setValue(valueStr);

		globalConfigData.server.pushMessageToClients(msg);
	}
}
