#include "config_server.h"

#include "caer-sdk/cross/portable_io.h"
#include "caer-sdk/cross/portable_threads.h"

#include "config_updater.h"

#include <algorithm>
#include <boost/version.hpp>
#include <string>
#include <system_error>

namespace logger = libcaer::log;

static void caerConfigServerRestartListener(sshsNode node, void *userData, enum sshs_node_attribute_events event,
	const char *changeKey, enum sshs_node_attr_value_type changeType, union sshs_node_attr_value changeValue);

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
	pushClients.push_back(pushClient);
}

void ConfigServer::removePushClient(ConfigServerConnection *pushClient) {
	pushClients.erase(std::remove(pushClients.begin(), pushClients.end(), pushClient), pushClients.end());
}

void ConfigServer::serviceConfigure() {
	// Get config.
	sshsNode serverNode = sshsGetNode(sshsGetGlobal(), "/caer/server/");

	// Configure acceptor.
	auto endpoint = asioTCP::endpoint(asioIP::address::from_string(sshsNodeGetStdString(serverNode, "ipAddress")),
		U16T(sshsNodeGetInt(serverNode, "portNumber")));

	acceptor.open(endpoint.protocol());
	acceptor.set_option(asioTCP::socket::reuse_address(true));
	acceptor.bind(endpoint);
	acceptor.listen();

	// Configure SSL support.
	sslEnabled = sshsNodeGetBool(serverNode, "ssl");

	if (sslEnabled) {
		try {
			sslContext.use_certificate_chain_file(sshsNodeGetStdString(serverNode, "sslCertFile"));
		}
		catch (const boost::system::system_error &ex) {
			logger::log(logger::logLevel::ERROR, CONFIG_SERVER_NAME,
				"Failed to load certificate file (error '%s'), disabling SSL.", ex.what());
			sslEnabled = false;
			return;
		}

		try {
			sslContext.use_private_key_file(
				sshsNodeGetStdString(serverNode, "sslKeyFile"), boost::asio::ssl::context::pem);
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

		if (sshsNodeGetBool(serverNode, "sslClientVerification")) {
			const std::string sslVerifyFile = sshsNodeGetStdString(serverNode, "sslClientVerificationFile");

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

	// Run IO service.
	ioService.run();
}

void ConfigServer::serviceStop() {
	// Prevent multiple calls.
	if (ioThreadState != IOThreadState::RUNNING) {
		return;
	}

	ioThreadState = IOThreadState::STOPPING;

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
				= std::make_shared<ConfigServerConnection>(std::move(acceptorNewSocket), sslEnabled, sslContext, this);

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
	sshsNode serverNode = sshsGetNode(sshsGetGlobal(), "/caer/server/");

	// Support restarting the config server.
	sshsNodeCreate(serverNode, "restart", false, SSHS_FLAGS_NOTIFY_ONLY | SSHS_FLAGS_NO_EXPORT,
		"Restart configuration server, disconnects all clients and reloads itself.");

	// Ensure default values are present for IP/Port.
	sshsNodeCreate(serverNode, "ipAddress", "127.0.0.1", 2, 39, SSHS_FLAGS_NORMAL,
		"IP address to listen on for configuration server connections.");
	sshsNodeCreate(serverNode, "portNumber", 4040, 1, UINT16_MAX, SSHS_FLAGS_NORMAL,
		"Port to listen on for configuration server connections.");

	// Default values for SSL encryption support.
	sshsNodeCreate(
		serverNode, "ssl", false, SSHS_FLAGS_NORMAL, "Require SSL encryption for configuration server communication.");
	sshsNodeCreate(
		serverNode, "sslCertFile", "", 0, PATH_MAX, SSHS_FLAGS_NORMAL, "Path to SSL certificate file (PEM format).");
	sshsNodeCreate(
		serverNode, "sslKeyFile", "", 0, PATH_MAX, SSHS_FLAGS_NORMAL, "Path to SSL private key file (PEM format).");

	sshsNodeCreate(
		serverNode, "sslClientVerification", false, SSHS_FLAGS_NORMAL, "Require SSL client certificate verification.");
	sshsNodeCreate(serverNode, "sslClientVerificationFile", "", 0, PATH_MAX, SSHS_FLAGS_NORMAL,
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
	sshsNodeAddAttributeListener(serverNode, nullptr, &caerConfigServerRestartListener);

	// Successfully started threads.
	logger::log(logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Threads created successfully.");
}

void caerConfigServerStop(void) {
	sshsNode serverNode = sshsGetNode(sshsGetGlobal(), "/caer/server/");

	// Remove restart listener first.
	sshsNodeRemoveAttributeListener(serverNode, nullptr, &caerConfigServerRestartListener);

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

static void caerConfigServerRestartListener(sshsNode node, void *userData, enum sshs_node_attribute_events event,
	const char *changeKey, enum sshs_node_attr_value_type changeType, union sshs_node_attr_value changeValue) {
	UNUSED_ARGUMENT(node);
	UNUSED_ARGUMENT(userData);

	if (event == SSHS_ATTRIBUTE_MODIFIED && changeType == SSHS_BOOL && caerStrEquals(changeKey, "restart")
		&& changeValue.boolean) {
		globalConfigData.server.serviceRestart();
	}
}
