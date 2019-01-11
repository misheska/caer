#include "config_server_connection.h"

#include "config_server.h"
#include "config_server_actions.h"
#include "config_server_main.h"

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

void ConfigServerConnection::writePushMessage(std::shared_ptr<const ConfigActionData> message) {
	auto self(shared_from_this());

	socket.write(asio::buffer(message->getBuffer(), message->size()),
		[this, self, message](const boost::system::error_code &error, size_t /*length*/) {
			if (error) {
				handleError(error, "Failed to write push message");
			}
		});
}

ConfigActionData &ConfigServerConnection::getData() {
	return (data);
}

void ConfigServerConnection::writeResponse() {
	auto self(shared_from_this());

	socket.write(asio::buffer(data.getBuffer(), data.size()),
		[this, self](const boost::system::error_code &error, size_t /*length*/) {
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

	socket.read(asio::buffer(data.getHeaderBuffer(), data.headerSize()),
		[this, self](const boost::system::error_code &error, size_t /*length*/) {
			if (error) {
				handleError(error, "Failed to read header");
			}
			else {
				// Check for wrong (excessive) requested read length.
				// Close connection by falling out of scope.
				if (data.dataSize() > (CAER_CONFIG_SERVER_BUFFER_SIZE - CAER_CONFIG_SERVER_HEADER_SIZE)) {
					logger::log(logger::logLevel::INFO, CONFIG_SERVER_NAME,
						"Client %s:%d: read length error (%d bytes requested).",
						socket.remote_address().to_string().c_str(), socket.remote_port(), data.dataSize());
					return;
				}

				readData();
			}
		});
}

void ConfigServerConnection::readData() {
	auto self(shared_from_this());

	socket.read(asio::buffer(data.getDataBuffer(), data.dataSize()),
		[this, self](const boost::system::error_code &error, size_t /*length*/) {
			if (error) {
				handleError(error, "Failed to read data");
			}
			else {
				caerConfigServerHandleRequest(self);
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
