#include "config_server_connection.h"

#include "config_server.h"
#include "config_server_actions.h"

namespace logger = libcaer::log;

ConfigServerConnection::ConfigServerConnection(
	asioTCP::socket s, bool sslEnabled, asioSSL::context &sslContext, ConfigServer *server) :
	parent(server),
	socket(std::move(s), sslEnabled, sslContext) {
	logger::log(logger::logLevel::INFO, CONFIG_SERVER_NAME, "New connection from client %s:%d.",
		socket.remote_address().to_string().c_str(), socket.remote_port());
}

ConfigServerConnection::~ConfigServerConnection() {
	parent->removeClient(this);

	logger::log(logger::logLevel::INFO, CONFIG_SERVER_NAME, "Closing connection from client %s:%d.",
		socket.remote_address().to_string().c_str(), socket.remote_port());
}

void ConfigServerConnection::start() {
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

void ConfigServerConnection::close() {
	socket.close();
}

void ConfigServerConnection::addPushClient() {
	parent->addPushClient(this);
}

void ConfigServerConnection::removePushClient() {
	parent->removePushClient(this);
}

uint8_t *ConfigServerConnection::getData() {
	return (data);
}

void ConfigServerConnection::writeResponse(size_t dataLength) {
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

void ConfigServerConnection::readHeader() {
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

void ConfigServerConnection::readData(size_t dataLength) {
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
				const uint8_t *key
					= (keyLength == 0) ? (nullptr) : (data + CAER_CONFIG_SERVER_HEADER_SIZE + extraLength + nodeLength);
				const uint8_t *value
					= (valueLength == 0)
						  ? (nullptr)
						  : (data + CAER_CONFIG_SERVER_HEADER_SIZE + extraLength + nodeLength + keyLength);

				caerConfigServerHandleRequest(
					self, action, type, extra, extraLength, node, nodeLength, key, keyLength, value, valueLength);
			}
		});
}

void ConfigServerConnection::handleError(const boost::system::error_code &error, const char *message) {
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
