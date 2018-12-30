#include "config_server.h"

#include <libcaercpp/libcaer.hpp>

#include "caer-sdk/cross/portable_io.h"
#include "caer-sdk/cross/portable_threads.h"

#include "mainloop.h"

#include <algorithm>
#include <atomic>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/format.hpp>
#include <memory>
#include <mutex>
#include <new>
#include <regex>
#include <shared_mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
namespace logger = libcaer::log;

namespace asio    = boost::asio;
namespace asioSSL = boost::asio::ssl;
namespace asioIP  = boost::asio::ip;
using asioTCP     = boost::asio::ip::tcp;

#define CONFIG_SERVER_NAME "Config Server"

class ConfigServerConnection;

static void caerConfigServerHandleRequest(std::shared_ptr<ConfigServerConnection> client, uint8_t action, uint8_t type,
	const uint8_t *extra, size_t extraLength, const uint8_t *node, size_t nodeLength, const uint8_t *key,
	size_t keyLength, const uint8_t *value, size_t valueLength);

class TCPSSLOrderedSocket {
private:
	asioSSL::stream<asioTCP::socket> socket;
	bool sslConnection;
	bool sslInitialized;

public:
	TCPSSLOrderedSocket(asioTCP::socket s, bool sslEnabled, asioSSL::context &sslContext) :
		socket(std::move(s), sslContext),
		sslConnection(sslEnabled),
		sslInitialized(false) {
	}

	~TCPSSLOrderedSocket() {
		// Shutdown SSL cleanly, if enabled and initialized successfully.
		if (sslConnection && sslInitialized) {
			socket.shutdown();
		}

		// Close underlying TCP socket cleanly.
		// TCP shutdown() should be called for portability.
		next_layer().shutdown(asioTCP::socket::shutdown_both);
		next_layer().close();
	}

	const asioTCP::socket &next_layer() const {
		return (socket.next_layer());
	}

	asioTCP::socket &next_layer() {
		return (socket.next_layer());
	}

	/**
	 * Startup handler needs following signature:
	 * void (const boost::system::error_code &)
	 */
	template<typename StartupHandler> void start(StartupHandler &&stHandler) {
		if (sslConnection) {
			socket.async_handshake(
				asioSSL::stream_base::server, [this, stHandler](const boost::system::error_code &error) {
					if (!error) {
						sslInitialized = true;
					}

					stHandler(error);
				});
		}
		else {
			stHandler(boost::system::error_code());
		}
	}

	/**
	 * Write handler needs following signature:
	 * void (const boost::system::error_code &, size_t)
	 */
	template<typename WriteHandler> void write(const asio::const_buffer &buf, WriteHandler &&wrHandler) {
		if (sslConnection) {
			asio::async_write(socket, buf, wrHandler);
		}
		else {
			asio::async_write(next_layer(), buf, wrHandler);
		}
	}

	/**
	 * Read handler needs following signature:
	 * void (const boost::system::error_code &, size_t)
	 */
	template<typename ReadHandler> void read(const asio::mutable_buffer &buf, ReadHandler &&rdHandler) {
		if (sslConnection) {
			asio::async_read(socket, buf, rdHandler);
		}
		else {
			asio::async_read(next_layer(), buf, rdHandler);
		}
	}

	asioTCP::endpoint local_endpoint() const {
		return (next_layer().local_endpoint());
	}

	asioIP::address local_address() const {
		return (local_endpoint().address());
	}

	uint16_t local_port() const {
		return (local_endpoint().port());
	}

	asioTCP::endpoint remote_endpoint() const {
		return (next_layer().remote_endpoint());
	}

	asioIP::address remote_address() const {
		return (remote_endpoint().address());
	}

	uint16_t remote_port() const {
		return (remote_endpoint().port());
	}
};

class ConfigServerConnection : public std::enable_shared_from_this<ConfigServerConnection> {
private:
	TCPSSLOrderedSocket socket;
	uint8_t data[CAER_CONFIG_SERVER_BUFFER_SIZE];

public:
	ConfigServerConnection(asioTCP::socket s, bool sslEnabled, asioSSL::context &sslContext) :
		socket(std::move(s), sslEnabled, sslContext) {
		logger::log(logger::logLevel::INFO, CONFIG_SERVER_NAME, "New connection from client %s:%d.",
			socket.remote_address().to_string().c_str(), socket.remote_port());
	}

	~ConfigServerConnection() {
		logger::log(logger::logLevel::INFO, CONFIG_SERVER_NAME, "Closing connection from client %s:%d.",
			socket.remote_address().to_string().c_str(), socket.remote_port());
	}

	void start() {
		auto self(shared_from_this());

		socket.start([this, self](const boost::system::error_code &error) {
			if (error) {
				handleError(error, "Failed startup (SSL handshake)");
			}
			else {
				readHeader();
			}
		});
	}

	uint8_t *getData() {
		return (data);
	}

	void writeResponse(size_t dataLength) {
		auto self(shared_from_this());

		socket.write(
			asio::buffer(data, dataLength), [this, self](const boost::system::error_code &error, size_t /*length*/) {
				if (error) {
					handleError(error, "Failed to write response");
				}
				else {
					// Restart.
					readHeader();
				}
			});
	}

private:
	void readHeader() {
		auto self(shared_from_this());

		socket.read(asio::buffer(data, CAER_CONFIG_SERVER_HEADER_SIZE),
			[this, self](const boost::system::error_code &error, size_t /*length*/) {
				if (error) {
					handleError(error, "Failed to read header");
				}
				else {
					// If we have enough data, we start parsing the lengths.
					// The main header is 10 bytes.
					// Decode length header fields (all in little-endian).
					uint16_t extraLength = le16toh(*(uint16_t *) (data + 2));
					uint16_t nodeLength  = le16toh(*(uint16_t *) (data + 4));
					uint16_t keyLength   = le16toh(*(uint16_t *) (data + 6));
					uint16_t valueLength = le16toh(*(uint16_t *) (data + 8));

					// Total length to get for command.
					size_t readLength = (size_t)(extraLength + nodeLength + keyLength + valueLength);

					// Check for wrong (excessive) requested read length.
					// Close connection by falling out of scope.
					if (readLength > (CAER_CONFIG_SERVER_BUFFER_SIZE - CAER_CONFIG_SERVER_HEADER_SIZE)) {
						logger::log(logger::logLevel::INFO, CONFIG_SERVER_NAME,
							"Client %s:%d: read length error (%d bytes requested).",
							socket.remote_address().to_string().c_str(), socket.remote_port(), readLength);
						return;
					}

					readData(readLength);
				}
			});
	}

	void readData(size_t dataLength) {
		auto self(shared_from_this());

		socket.read(asio::buffer(data + CAER_CONFIG_SERVER_HEADER_SIZE, dataLength),
			[this, self](const boost::system::error_code &error, size_t /*length*/) {
				if (error) {
					handleError(error, "Failed to read data");
				}
				else {
					// Decode command header fields.
					uint8_t action = data[0];
					uint8_t type   = data[1];

					// Decode length header fields (all in little-endian).
					uint16_t extraLength = le16toh(*(uint16_t *) (data + 2));
					uint16_t nodeLength  = le16toh(*(uint16_t *) (data + 4));
					uint16_t keyLength   = le16toh(*(uint16_t *) (data + 6));
					uint16_t valueLength = le16toh(*(uint16_t *) (data + 8));

					// Now we have everything. The header fields are already
					// fully decoded: handle request (and send back data eventually).
					const uint8_t *extra = (extraLength == 0) ? (nullptr) : (data + CAER_CONFIG_SERVER_HEADER_SIZE);
					const uint8_t *node
						= (nodeLength == 0) ? (nullptr) : (data + CAER_CONFIG_SERVER_HEADER_SIZE + extraLength);
					const uint8_t *key = (keyLength == 0)
											 ? (nullptr)
											 : (data + CAER_CONFIG_SERVER_HEADER_SIZE + extraLength + nodeLength);
					const uint8_t *value
						= (valueLength == 0)
							  ? (nullptr)
							  : (data + CAER_CONFIG_SERVER_HEADER_SIZE + extraLength + nodeLength + keyLength);

					caerConfigServerHandleRequest(
						self, action, type, extra, extraLength, node, nodeLength, key, keyLength, value, valueLength);
				}
			});
	}

	void handleError(const boost::system::error_code &error, const char *message) {
		if (error == asio::error::eof) {
			// Handle EOF separately.
			logger::log(logger::logLevel::INFO, CONFIG_SERVER_NAME, "Client %s:%d: connection closed.",
				socket.remote_address().to_string().c_str(), socket.remote_port());
		}
		else {
			logger::log(logger::logLevel::ERROR, CONFIG_SERVER_NAME, "Client %s:%d: %s. Error: %s (%d).",
				socket.remote_address().to_string().c_str(), socket.remote_port(), message, error.message().c_str(),
				error.value());
		}
	}
};

class ConfigServer {
private:
	asio::io_service ioService;
	asioSSL::context sslContext;
	asioTCP::acceptor acceptor;
	std::thread ioThread;
	bool sslConnection;

public:
	ConfigServer() : sslContext(asioSSL::context::tlsv12_server), acceptor(ioService), sslConnection(false) {
	}

	ConfigServer(const asioIP::address &listenAddress, unsigned short listenPort, bool sslEnabled,
		const std::string &sslCertFile, const std::string &sslKeyFile, bool sslClientVerification,
		const std::string &sslVerifyFile) :
		sslContext(asioSSL::context::tlsv12_server),
		acceptor(ioService, asioTCP::endpoint(listenAddress, listenPort)),
		sslConnection(sslEnabled) {
		if (sslEnabled) {
			try {
				sslContext.use_certificate_chain_file(sslCertFile);
			}
			catch (const boost::system::system_error &ex) {
				logger::log(logger::logLevel::ERROR, CONFIG_SERVER_NAME,
					"Failed to load certificate file (error '%s'), disabling SSL.", ex.what());
				sslConnection = false;
				return;
			}

			try {
				sslContext.use_private_key_file(sslKeyFile, boost::asio::ssl::context::pem);
			}
			catch (const boost::system::system_error &ex) {
				logger::log(logger::logLevel::ERROR, CONFIG_SERVER_NAME,
					"Failed to load private key file (error '%s'), disabling SSL.", ex.what());
				sslConnection = false;
				return;
			}

			sslContext.set_options(
				boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::single_dh_use);

			if (sslClientVerification) {
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

				sslContext.set_verify_mode(
					asioSSL::context::verify_peer | asioSSL::context::verify_fail_if_no_peer_cert);
			}
		}
	}

	void start() {
		acceptStart();

		threadStart();
	}

	void stop() {
		threadStop();
	}

private:
	void acceptStart() {
		acceptor.async_accept([this](const boost::system::error_code &error, asioTCP::socket socket) {
			if (error) {
				logger::log(logger::logLevel::ERROR, CONFIG_SERVER_NAME,
					"Failed to accept new connection. Error: %s (%d).", error.message().c_str(), error.value());
			}
			else {
				std::make_shared<ConfigServerConnection>(std::move(socket), sslConnection, sslContext)->start();
			}

			acceptStart();
		});
	}

	void threadStart() {
		ioThread = std::thread([this]() {
			// Set thread name.
			portable_thread_set_name("ConfigServer");

			// Run IO service.
			while (!ioService.stopped()) {
				ioService.run();
			}
		});
	}

	void threadStop() {
		ioService.stop();

		ioThread.join();
	}
};

class ConfigUpdater {
private:
	std::thread updateThread;
	std::atomic_bool runThread;
	sshs configTree;

public:
	ConfigUpdater() : ConfigUpdater(sshsGetGlobal()) {
	}

	ConfigUpdater(sshs tree) : configTree(tree) {
	}

	void start() {
		threadStart();
	}

	void stop() {
		threadStop();
	}

private:
	void threadStart() {
		runThread.store(true);

		updateThread = std::thread([this]() {
			// Set thread name.
			portable_thread_set_name("ConfigUpdater");

			while (runThread.load()) {
				sshsAttributeUpdaterRun(configTree);

				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		});
	}

	void threadStop() {
		runThread.store(false);

		updateThread.join();
	}
};

static struct {
	ConfigServer server;
	ConfigUpdater configUpdater;
} glConfigServerData;

void caerConfigServerStart(void) {
	// Get the right configuration node first.
	sshsNode serverNode = sshsGetNode(sshsGetGlobal(), "/caer/server/");

	// Ensure default values are present.
	sshsNodeCreate(serverNode, "ipAddress", "127.0.0.1", 7, 15, SSHS_FLAGS_NORMAL,
		"IPv4 address to listen on for configuration server connections.");
	sshsNodeCreate(serverNode, "portNumber", CAER_CONFIG_SERVER_DEFAULT_PORT, 1, UINT16_MAX, SSHS_FLAGS_NORMAL,
		"Port to listen on for configuration server connections.");

	sshsNodeCreate(
		serverNode, "ssl", false, SSHS_FLAGS_NORMAL, "Require SSL encryption for config-server communication.");
	sshsNodeCreate(
		serverNode, "sslCertFile", "", 0, PATH_MAX, SSHS_FLAGS_NORMAL, "Path to SSL certificate file (PEM format).");
	sshsNodeCreate(
		serverNode, "sslKeyFile", "", 0, PATH_MAX, SSHS_FLAGS_NORMAL, "Path to SSL private key file (PEM format).");

	sshsNodeCreate(
		serverNode, "sslClientVerification", false, SSHS_FLAGS_NORMAL, "Require SSL client certificate verification.");
	sshsNodeCreate(serverNode, "sslClientVerificationFile", "", 0, PATH_MAX, SSHS_FLAGS_NORMAL,
		"Path to SSL CA file for client verification (PEM format). Leave empty to use system defaults.");

	// Start the threads.
	try {
		new (&glConfigServerData.server)
			ConfigServer(asioIP::address::from_string(sshsNodeGetStdString(serverNode, "ipAddress")),
				U16T(sshsNodeGetInt(serverNode, "portNumber")), sshsNodeGetBool(serverNode, "ssl"),
				sshsNodeGetStdString(serverNode, "sslCertFile"), sshsNodeGetStdString(serverNode, "sslKeyFile"),
				sshsNodeGetBool(serverNode, "sslClientVerification"),
				sshsNodeGetStdString(serverNode, "sslClientVerificationFile"));

		new (&glConfigServerData.configUpdater) ConfigUpdater(sshsGetGlobal());

		glConfigServerData.server.start();

		glConfigServerData.configUpdater.start();
	}
	catch (const std::system_error &ex) {
		// Failed to create threads.
		logger::log(logger::logLevel::EMERGENCY, CONFIG_SERVER_NAME, "Failed to create threads. Error: %s.", ex.what());
		exit(EXIT_FAILURE);
	}

	// Successfully started threads.
	logger::log(logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Threads created successfully.");
}

void caerConfigServerStop(void) {
	try {
		// Stop Config Updater thread.
		glConfigServerData.configUpdater.stop();

		glConfigServerData.server.stop();
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

// The response from the server follows a simplified version of the request
// protocol. A byte for ACTION, a byte for TYPE, 2 bytes for MSG_LEN and then
// up to 4092 bytes of MSG, for a maximum total of 4096 bytes again.
// MSG must be NUL terminated, and the NUL byte shall be part of the length.
static inline void setMsgLen(uint8_t *buf, uint16_t msgLen) {
	*((uint16_t *) (buf + 2)) = htole16(msgLen);
}

static inline void caerConfigSendError(std::shared_ptr<ConfigServerConnection> client, const char *errorMsg) {
	size_t errorMsgLength = strlen(errorMsg);
	size_t responseLength = 4 + errorMsgLength + 1; // +1 for terminating NUL byte.

	uint8_t *response = client->getData();

	response[0] = CAER_CONFIG_ERROR;
	response[1] = SSHS_STRING;
	setMsgLen(response, (uint16_t)(errorMsgLength + 1));
	memcpy(response + 4, errorMsg, errorMsgLength);
	response[4 + errorMsgLength] = '\0';

	client->writeResponse(responseLength);

	logger::log(logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Sent back error message '%s' to client.", errorMsg);
}

static inline void caerConfigSendResponse(std::shared_ptr<ConfigServerConnection> client, uint8_t action, uint8_t type,
	const uint8_t *msg, size_t msgLength) {
	size_t responseLength = 4 + msgLength;

	uint8_t *response = client->getData();

	response[0] = action;
	response[1] = type;
	setMsgLen(response, (uint16_t) msgLength);
	memcpy(response + 4, msg, msgLength);
	// Msg must already be NUL terminated!

	client->writeResponse(responseLength);

	logger::log(logger::logLevel::DEBUG, CONFIG_SERVER_NAME,
		"Sent back message to client: action=%" PRIu8 ", type=%" PRIu8 ", msgLength=%zu.", action, type, msgLength);
}

static inline bool checkNodeExists(sshs configStore, const char *node, std::shared_ptr<ConfigServerConnection> client) {
	bool nodeExists = sshsExistsNode(configStore, node);

	// Only allow operations on existing nodes, this is for remote
	// control, so we only manipulate what's already there!
	if (!nodeExists) {
		// Send back error message to client.
		caerConfigSendError(client, "Node doesn't exist. Operations are only allowed on existing data.");
	}

	return (nodeExists);
}

static inline bool checkAttributeExists(sshsNode wantedNode, const char *key, enum sshs_node_attr_value_type type,
	std::shared_ptr<ConfigServerConnection> client) {
	// Check if attribute exists. Only allow operations on existing attributes!
	bool attrExists = sshsNodeAttributeExists(wantedNode, key, type);

	if (!attrExists) {
		// Send back error message to client.
		caerConfigSendError(
			client, "Attribute of given type doesn't exist. Operations are only allowed on existing data.");
	}

	return (attrExists);
}

static inline void caerConfigSendBoolResponse(
	std::shared_ptr<ConfigServerConnection> client, uint8_t action, bool result) {
	// Send back result to client. Format is the same as incoming data.
	const uint8_t *sendResult = (const uint8_t *) ((result) ? ("true") : ("false"));
	size_t sendResultLength   = (result) ? (5) : (6);
	caerConfigSendResponse(client, action, SSHS_BOOL, sendResult, sendResultLength);
}

static void caerConfigServerHandleRequest(std::shared_ptr<ConfigServerConnection> client, uint8_t action, uint8_t type,
	const uint8_t *extra, size_t extraLength, const uint8_t *node, size_t nodeLength, const uint8_t *key,
	size_t keyLength, const uint8_t *value, size_t valueLength) {
	UNUSED_ARGUMENT(extra);

	logger::log(logger::logLevel::DEBUG, CONFIG_SERVER_NAME,
		"Handling request: action=%" PRIu8 ", type=%" PRIu8
		", extraLength=%zu, nodeLength=%zu, keyLength=%zu, valueLength=%zu.",
		action, type, extraLength, nodeLength, keyLength, valueLength);

	// Interpretation of data is up to each action individually.
	sshs configStore = sshsGetGlobal();

	switch (action) {
		case CAER_CONFIG_NODE_EXISTS: {
			// We only need the node name here. Type is not used (ignored)!
			bool result = sshsExistsNode(configStore, (const char *) node);

			// Send back result to client. Format is the same as incoming data.
			caerConfigSendBoolResponse(client, CAER_CONFIG_NODE_EXISTS, result);

			break;
		}

		case CAER_CONFIG_ATTR_EXISTS: {
			if (!checkNodeExists(configStore, (const char *) node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, (const char *) node);

			// Check if attribute exists.
			bool result
				= sshsNodeAttributeExists(wantedNode, (const char *) key, (enum sshs_node_attr_value_type) type);

			// Send back result to client. Format is the same as incoming data.
			caerConfigSendBoolResponse(client, CAER_CONFIG_ATTR_EXISTS, result);

			break;
		}

		case CAER_CONFIG_GET: {
			if (!checkNodeExists(configStore, (const char *) node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, (const char *) node);

			if (!checkAttributeExists(wantedNode, (const char *) key, (enum sshs_node_attr_value_type) type, client)) {
				break;
			}

			union sshs_node_attr_value result
				= sshsNodeGetAttribute(wantedNode, (const char *) key, (enum sshs_node_attr_value_type) type);

			char *resultStr = sshsHelperValueToStringConverter((enum sshs_node_attr_value_type) type, result);

			if (resultStr == nullptr) {
				// Send back error message to client.
				caerConfigSendError(client, "Failed to allocate memory for value string.");
			}
			else {
				caerConfigSendResponse(
					client, CAER_CONFIG_GET, type, (const uint8_t *) resultStr, strlen(resultStr) + 1);

				free(resultStr);
			}

			// If this is a string, we must remember to free the original result.str
			// too, since it will also be a copy of the string coming from SSHS.
			if (type == SSHS_STRING) {
				free(result.string);
			}

			break;
		}

		case CAER_CONFIG_PUT: {
			if (!checkNodeExists(configStore, (const char *) node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, (const char *) node);

			if (!checkAttributeExists(wantedNode, (const char *) key, (enum sshs_node_attr_value_type) type, client)) {
				break;
			}

			// Put given value into config node. Node, attr and type are already verified.
			const char *typeStr = sshsHelperTypeToStringConverter((enum sshs_node_attr_value_type) type);
			if (!sshsNodeStringToAttributeConverter(wantedNode, (const char *) key, typeStr, (const char *) value)) {
				// Send back correct error message to client.
				if (errno == EINVAL) {
					caerConfigSendError(client, "Impossible to convert value according to type.");
				}
				else if (errno == EPERM) {
					caerConfigSendError(client, "Cannot write to a read-only attribute.");
				}
				else if (errno == ERANGE) {
					caerConfigSendError(client, "Value out of attribute range.");
				}
				else {
					// Unknown error.
					caerConfigSendError(client, "Unknown error.");
				}

				break;
			}

			// Send back confirmation to the client.
			caerConfigSendBoolResponse(client, CAER_CONFIG_PUT, true);

			break;
		}

		case CAER_CONFIG_GET_CHILDREN: {
			if (!checkNodeExists(configStore, (const char *) node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, (const char *) node);

			// Get the names of all the child nodes and return them.
			size_t numNames;
			const char **childNames = sshsNodeGetChildNames(wantedNode, &numNames);

			// No children at all, return empty.
			if (childNames == nullptr) {
				// Send back error message to client.
				caerConfigSendError(client, "Node has no children.");

				break;
			}

			// We need to return a big string with all of the child names,
			// separated by a NUL character.
			size_t namesLength = 0;

			for (size_t i = 0; i < numNames; i++) {
				namesLength += strlen(childNames[i]) + 1; // +1 for terminating NUL byte.
			}

			// Allocate a buffer for the names and copy them over.
			char namesBuffer[namesLength];

			for (size_t i = 0, acc = 0; i < numNames; i++) {
				size_t len = strlen(childNames[i]) + 1;
				memcpy(namesBuffer + acc, childNames[i], len);
				acc += len;
			}

			free(childNames);

			caerConfigSendResponse(
				client, CAER_CONFIG_GET_CHILDREN, SSHS_STRING, (const uint8_t *) namesBuffer, namesLength);

			break;
		}

		case CAER_CONFIG_GET_ATTRIBUTES: {
			if (!checkNodeExists(configStore, (const char *) node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, (const char *) node);

			// Get the keys of all the attributes and return them.
			size_t numKeys;
			const char **attrKeys = sshsNodeGetAttributeKeys(wantedNode, &numKeys);

			// No attributes at all, return empty.
			if (attrKeys == nullptr) {
				// Send back error message to client.
				caerConfigSendError(client, "Node has no attributes.");

				break;
			}

			// We need to return a big string with all of the attribute keys,
			// separated by a NUL character.
			size_t keysLength = 0;

			for (size_t i = 0; i < numKeys; i++) {
				keysLength += strlen(attrKeys[i]) + 1; // +1 for terminating NUL byte.
			}

			// Allocate a buffer for the keys and copy them over.
			char keysBuffer[keysLength];

			for (size_t i = 0, acc = 0; i < numKeys; i++) {
				size_t len = strlen(attrKeys[i]) + 1;
				memcpy(keysBuffer + acc, attrKeys[i], len);
				acc += len;
			}

			free(attrKeys);

			caerConfigSendResponse(
				client, CAER_CONFIG_GET_ATTRIBUTES, SSHS_STRING, (const uint8_t *) keysBuffer, keysLength);

			break;
		}

		case CAER_CONFIG_GET_TYPE: {
			if (!checkNodeExists(configStore, (const char *) node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, (const char *) node);

			// Check if any keys match the given one and return its type.
			enum sshs_node_attr_value_type attrType = sshsNodeGetAttributeType(wantedNode, (const char *) key);

			// No attributes for specified key, return empty.
			if (attrType == SSHS_UNKNOWN) {
				// Send back error message to client.
				caerConfigSendError(client, "Node has no attributes with specified key.");

				break;
			}

			// We need to return a string with the attribute type,
			// separated by a NUL character.
			const char *typeStr = sshsHelperTypeToStringConverter(attrType);

			caerConfigSendResponse(
				client, CAER_CONFIG_GET_TYPE, SSHS_STRING, (const uint8_t *) typeStr, strlen(typeStr) + 1);

			break;
		}

		case CAER_CONFIG_GET_RANGES: {
			if (!checkNodeExists(configStore, (const char *) node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, (const char *) node);

			if (!checkAttributeExists(wantedNode, (const char *) key, (enum sshs_node_attr_value_type) type, client)) {
				break;
			}

			struct sshs_node_attr_ranges ranges
				= sshsNodeGetAttributeRanges(wantedNode, (const char *) key, (enum sshs_node_attr_value_type) type);

			// We need to return a string with the two ranges,
			// separated by a NUL character.
			char buf[256];
			size_t bufLen = 0;

			switch (type) {
				case SSHS_BOOL:
					bufLen = 4;
					memcpy(buf, "0\00\0", bufLen);
					break;

				case SSHS_INT:
					bufLen += (size_t) snprintf(buf + bufLen, 256 - bufLen, "%" PRIi32, ranges.min.iintRange)
							  + 1; // Terminating NUL byte.
					bufLen += (size_t) snprintf(buf + bufLen, 256 - bufLen, "%" PRIi32, ranges.max.iintRange)
							  + 1; // Terminating NUL byte.
					break;

				case SSHS_LONG:
					bufLen += (size_t) snprintf(buf + bufLen, 256 - bufLen, "%" PRIi64, ranges.min.ilongRange)
							  + 1; // Terminating NUL byte.
					bufLen += (size_t) snprintf(buf + bufLen, 256 - bufLen, "%" PRIi64, ranges.max.ilongRange)
							  + 1; // Terminating NUL byte.
					break;

				case SSHS_FLOAT:
					bufLen += (size_t) snprintf(buf + bufLen, 256 - bufLen, "%g", (double) ranges.min.ffloatRange)
							  + 1; // Terminating NUL byte.
					bufLen += (size_t) snprintf(buf + bufLen, 256 - bufLen, "%g", (double) ranges.max.ffloatRange)
							  + 1; // Terminating NUL byte.
					break;

				case SSHS_DOUBLE:
					bufLen += (size_t) snprintf(buf + bufLen, 256 - bufLen, "%g", ranges.min.ddoubleRange)
							  + 1; // Terminating NUL byte.
					bufLen += (size_t) snprintf(buf + bufLen, 256 - bufLen, "%g", ranges.max.ddoubleRange)
							  + 1; // Terminating NUL byte.
					break;

				case SSHS_STRING:
					bufLen += (size_t) snprintf(buf + bufLen, 256 - bufLen, "%zu", ranges.min.stringRange)
							  + 1; // Terminating NUL byte.
					bufLen += (size_t) snprintf(buf + bufLen, 256 - bufLen, "%zu", ranges.max.stringRange)
							  + 1; // Terminating NUL byte.
					break;
			}

			caerConfigSendResponse(client, CAER_CONFIG_GET_RANGES, type, (const uint8_t *) buf, bufLen);

			break;
		}

		case CAER_CONFIG_GET_FLAGS: {
			if (!checkNodeExists(configStore, (const char *) node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, (const char *) node);

			if (!checkAttributeExists(wantedNode, (const char *) key, (enum sshs_node_attr_value_type) type, client)) {
				break;
			}

			int flags
				= sshsNodeGetAttributeFlags(wantedNode, (const char *) key, (enum sshs_node_attr_value_type) type);

			std::string flagsStr;

			if (flags & SSHS_FLAGS_READ_ONLY) {
				flagsStr = "READ_ONLY";
			}
			else if (flags & SSHS_FLAGS_NOTIFY_ONLY) {
				flagsStr = "NOTIFY_ONLY";
			}
			else {
				flagsStr = "NORMAL";
			}

			if (flags & SSHS_FLAGS_NO_EXPORT) {
				flagsStr += ",NO_EXPORT";
			}

			caerConfigSendResponse(
				client, CAER_CONFIG_GET_FLAGS, SSHS_STRING, (const uint8_t *) flagsStr.c_str(), flagsStr.length() + 1);

			break;
		}

		case CAER_CONFIG_GET_DESCRIPTION: {
			if (!checkNodeExists(configStore, (const char *) node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, (const char *) node);

			if (!checkAttributeExists(wantedNode, (const char *) key, (enum sshs_node_attr_value_type) type, client)) {
				break;
			}

			char *description = sshsNodeGetAttributeDescription(
				wantedNode, (const char *) key, (enum sshs_node_attr_value_type) type);

			caerConfigSendResponse(client, CAER_CONFIG_GET_DESCRIPTION, SSHS_STRING, (const uint8_t *) description,
				strlen(description) + 1);

			free(description);

			break;
		}

		case CAER_CONFIG_ADD_MODULE: {
			if (nodeLength == 0) {
				// Disallow empty strings.
				caerConfigSendError(client, "Name cannot be empty.");
				break;
			}

			if (keyLength == 0) {
				// Disallow empty strings.
				caerConfigSendError(client, "Library cannot be empty.");
				break;
			}

			// Node is the module name, key the library.
			const std::string moduleName((const char *) node);
			const std::string moduleLibrary((const char *) key);

			// Check module name.
			if (moduleName == "caer") {
				caerConfigSendError(client, "Name is reserved for system use.");
				break;
			}

			const std::regex moduleNameRegex("^[a-zA-Z-_\\d\\.]+$");

			if (!std::regex_match(moduleName, moduleNameRegex)) {
				caerConfigSendError(client, "Name uses invalid characters.");
				break;
			}

			if (sshsExistsNode(configStore, "/" + moduleName + "/")) {
				caerConfigSendError(client, "Name is already in use.");
				break;
			}

			// Check module library.
			sshsNode modulesSysNode              = sshsGetNode(configStore, "/caer/modules/");
			const std::string modulesListOptions = sshsNodeGetStdString(modulesSysNode, "modulesListOptions");

			std::vector<std::string> modulesList;
			boost::algorithm::split(modulesList, modulesListOptions, boost::is_any_of(","));

			if (!findBool(modulesList.begin(), modulesList.end(), moduleLibrary)) {
				caerConfigSendError(client, "Library does not exist.");
				break;
			}

			// Name and library are fine, let's determine the next free ID.
			size_t rootNodesSize;
			sshsNode *rootNodes = sshsNodeGetChildren(sshsGetNode(configStore, "/"), &rootNodesSize);

			std::vector<int16_t> usedModuleIDs;

			for (size_t i = 0; i < rootNodesSize; i++) {
				sshsNode mNode = rootNodes[i];

				if (!sshsNodeAttributeExists(mNode, "moduleId", SSHS_INT)) {
					continue;
				}

				int16_t moduleID = I16T(sshsNodeGetInt(mNode, "moduleId"));
				usedModuleIDs.push_back(moduleID);
			}

			free(rootNodes);

			vectorSortUnique(usedModuleIDs);

			size_t idx               = 0;
			int16_t nextFreeModuleID = 1;

			while (idx < usedModuleIDs.size() && usedModuleIDs[idx] == nextFreeModuleID) {
				idx++;
				nextFreeModuleID++;
			}

			// Let's create the needed configuration for the new module.
			sshsNode newModuleNode = sshsGetNode(configStore, "/" + moduleName + "/");

			sshsNodeCreate(newModuleNode, "moduleId", nextFreeModuleID, I16T(1), I16T(INT16_MAX), SSHS_FLAGS_READ_ONLY,
				"Module ID.");
			sshsNodeCreate(
				newModuleNode, "moduleLibrary", moduleLibrary, 1, PATH_MAX, SSHS_FLAGS_READ_ONLY, "Module library.");

			// Add moduleInput/moduleOutput as appropriate.
			sshsNode moduleSysNode = sshsGetRelativeNode(modulesSysNode, moduleLibrary + "/");
			std::string inputType  = sshsNodeGetStdString(moduleSysNode, "type");

			if (inputType != "INPUT") {
				// CAER_MODULE_OUTPUT / CAER_MODULE_PROCESSOR
				// moduleInput must exist for OUTPUT and PROCESSOR modules.
				sshsNodeCreate(
					newModuleNode, "moduleInput", "", 0, 1024, SSHS_FLAGS_NORMAL, "Module dynamic input definition.");
			}

			if (inputType != "OUTPUT") {
				// CAER_MODULE_INPUT / CAER_MODULE_PROCESSOR
				// moduleOutput must exist for INPUT and PROCESSOR modules, only
				// if their outputs are undefined (-1).
				if (sshsExistsRelativeNode(moduleSysNode, "outputStreams/0/")) {
					sshsNode outputNode0    = sshsGetRelativeNode(moduleSysNode, "outputStreams/0/");
					int32_t outputNode0Type = sshsNodeGetInt(outputNode0, "type");

					if (outputNode0Type == -1) {
						sshsNodeCreate(newModuleNode, "moduleOutput", "", 0, 1024, SSHS_FLAGS_NORMAL,
							"Module dynamic output definition.");
					}
				}
			}

			// Create static module configuration, so users can start
			// changing it right away after module add.
			caerModuleConfigInit(newModuleNode);

			// Send back confirmation to the client.
			caerConfigSendBoolResponse(client, CAER_CONFIG_ADD_MODULE, true);

			break;
		}

		case CAER_CONFIG_REMOVE_MODULE: {
			if (nodeLength == 0) {
				// Disallow empty strings.
				caerConfigSendError(client, "Name cannot be empty.");
				break;
			}

			// Node is the module name.
			const std::string moduleName((const char *) node);

			// Check module name.
			if (moduleName == "caer") {
				caerConfigSendError(client, "Name is reserved for system use.");
				break;
			}

			if (!sshsExistsNode(configStore, "/" + moduleName + "/")) {
				caerConfigSendError(client, "Name is not in use.");
				break;
			}

			// Modules can only be deleted while the mainloop is not running, to not
			// destroy data the system is relying on.
			bool isMainloopRunning = sshsNodeGetBool(sshsGetNode(configStore, "/"), "running");
			if (isMainloopRunning) {
				caerConfigSendError(client, "Mainloop is running.");
				break;
			}

			// Truly delete the node and all its children.
			sshsNodeRemoveNode(sshsGetNode(configStore, "/" + moduleName + "/"));

			// Send back confirmation to the client.
			caerConfigSendBoolResponse(client, CAER_CONFIG_REMOVE_MODULE, true);

			break;
		}

		default: {
			// Unknown action, send error back to client.
			caerConfigSendError(client, "Unknown action.");

			break;
		}
	}
}
