#include "config_server_connection.h"

#include "config_server.h"
#include "config_server_actions.h"
#include "config_server_main.hpp"

// Start at 1, 0 is reserved for system.
std::atomic_uint64_t ConfigServerConnection::clientIDGenerator{1};

ConfigServerConnection::ConfigServerConnection(
	asioTCP::socket s, bool tlsEnabled, asioSSL::context *tlsContext, ConfigServer *server) :
	parent(server),
	socket(std::move(s), tlsEnabled, tlsContext) {
	clientID = clientIDGenerator.fetch_add(1);

	parent->setCurrentClientID(clientID);

	dv::Log(dv::logLevel::INFO, "New connection from client %lld (%s:%d).", clientID,
		socket.remote_address().to_string().c_str(), socket.remote_port());
}

ConfigServerConnection::~ConfigServerConnection() {
	parent->setCurrentClientID(clientID);

	parent->removeClient(this);

	dv::Log(dv::logLevel::INFO, "Closing connection from client %lld (%s:%d).", clientID,
		socket.remote_address().to_string().c_str(), socket.remote_port());
}

void ConfigServerConnection::start() {
	auto self(shared_from_this());

	socket.start(
		[this, self](const boost::system::error_code &error) {
			if (error) {
				handleError(error, "Failed startup (TLS handshake)");
			}
			else {
				readMessageSize();
			}
		},
		asioSSL::stream_base::server);
}

void ConfigServerConnection::close() {
	socket.close();
}

uint64_t ConfigServerConnection::getClientID() {
	return (clientID);
}

void ConfigServerConnection::addPushClient() {
	parent->setCurrentClientID(clientID);

	parent->addPushClient(this);
}

void ConfigServerConnection::removePushClient() {
	parent->setCurrentClientID(clientID);

	parent->removePushClient(this);
}

void ConfigServerConnection::writePushMessage(std::shared_ptr<const flatbuffers::FlatBufferBuilder> message) {
	auto self(shared_from_this());

	socket.write(asio::buffer(message->GetBufferPointer(), message->GetSize()),
		[this, self, message](const boost::system::error_code &error, size_t /*length*/) {
			if (error) {
				handleError(error, "Failed to write push message");
			}
		});
}

void ConfigServerConnection::writeMessage(std::shared_ptr<const flatbuffers::FlatBufferBuilder> message) {
	auto self(shared_from_this());

	socket.write(asio::buffer(message->GetBufferPointer(), message->GetSize()),
		[this, self, message](const boost::system::error_code &error, size_t /*length*/) {
			if (error) {
				handleError(error, "Failed to write message");
			}
			else {
				// Restart, wait for next message.
				readMessageSize();
			}
		});
}

void ConfigServerConnection::readMessageSize() {
	auto self(shared_from_this());

	socket.read(asio::buffer(&incomingMessageSize, sizeof(incomingMessageSize)),
		[this, self](const boost::system::error_code &error, size_t /*length*/) {
			if (error) {
				handleError(error, "Failed to read message size");
			}
			else {
				parent->setCurrentClientID(clientID);

				// Check for wrong (excessive) message length.
				// Close connection by falling out of scope.
				if (incomingMessageSize > DV_CONFIG_SERVER_MAX_INCOMING_SIZE) {
					dv::Log(dv::logLevel::INFO, "Client %lld: message length error (%d bytes).", clientID,
						incomingMessageSize);
					return;
				}

				readMessage();
			}
		});
}

void ConfigServerConnection::readMessage() {
	auto self(shared_from_this());

	auto messageBufferPtr = new uint8_t[incomingMessageSize];

	socket.read(asio::buffer(messageBufferPtr, incomingMessageSize),
		[this, self, messageBufferPtr](const boost::system::error_code &error, size_t /*length*/) {
			auto messageBuffer = std::unique_ptr<uint8_t[]>(messageBufferPtr);

			if (error) {
				handleError(error, "Failed to read message");
			}
			else {
				// Any changes coming as a result of clients doing something
				// must originate as a result of a call to this function.
				// So we set the client ID for the current thread to the
				// current client value, so that any listeners will see it too.
				parent->setCurrentClientID(clientID);

				// Now we have the flatbuffer message and can verify it.
				flatbuffers::Verifier verifyMessage(messageBuffer.get(), incomingMessageSize);

				if (!dv::VerifyConfigActionDataBuffer(verifyMessage)) {
					// Failed verification, close connection by falling out of scope.
					dv::Log(dv::logLevel::INFO, "Client %lld: message verification error.", clientID);
					return;
				}

				ConfigServerHandleRequest(self, std::move(messageBuffer));
			}
		});
}

void ConfigServerConnection::handleError(const boost::system::error_code &error, const char *message) {
	parent->setCurrentClientID(clientID);

	if (error == asio::error::eof) {
		// Handle EOF separately.
		dv::Log(dv::logLevel::INFO, "Client %lld: connection closed.", clientID);
	}
	else {
		dv::Log(dv::logLevel::ERROR, "Client %lld: %s. Error: %s (%d).", clientID, message, error.message().c_str(),
			error.value());
	}
}
