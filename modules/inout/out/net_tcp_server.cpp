#include "dv-sdk/module.hpp"

#include "dv_output.hpp"
#include "src/config_server/asio.h"

#include <memory>

namespace logger = libcaer::log;

class NetTCPServer;

class Connection : public std::enable_shared_from_this<Connection> {
private:
	NetTCPServer *parent;
	TCPTLSWriteOrderedSocket socket;
	uint8_t keepAliveReadSpace;

public:
	Connection(asioTCP::socket s, NetTCPServer *server);
	~Connection();

	void start();
	void close();
	void writeBuffer(std::shared_ptr<const asio::const_buffer> buffer);

private:
	void keepAliveByReading();
	void handleError(const boost::system::error_code &error, const char *message);
};

class NetTCPServer : public dv::BaseModule {
private:
	asio::io_service ioService;
	asioTCP::acceptor acceptor;
	asioTCP::socket acceptorNewSocket;
	std::vector<Connection *> clients;

public:
	static constexpr caer_event_stream_in inputStreams[]  = {{-1, -1, true}};
	static constexpr caer_event_stream_out *outputStreams = nullptr;

	static const char *getDescription() {
		return ("Send AEDAT 4 data out via TCP to connected clients (server mode).");
	}

	static void getConfigOptions(std::map<std::string, dv::ConfigOption> &config) {
		config["ipAddress"] = dv::ConfigOption::stringOption("IPv4 address to listen on (server mode).", "127.0.0.1");
		config["portNumber"]
			= dv::ConfigOption::integerOption("Port number to listen on (server mode).", 7777, 0, UINT16_MAX);
		config["backlogSize"] = dv::ConfigOption::integerOption("Maximum number of pending connections.", 5, 1, 32);
		config["concurrentConnections"]
			= dv::ConfigOption::integerOption("Maximum number of concurrent active connections.", 10, 1, 128);
	}

	NetTCPServer() : acceptor(ioService), acceptorNewSocket(ioService) {
		// Configure acceptor.
		auto endpoint
			= asioTCP::endpoint(asioIP::address::from_string(config["ipAddress"].getValue<dv::ConfigVariant::STRING>()),
				U16T(config["portNumber"].getValue<dv::ConfigVariant::INTEGER>()));

		// TODO: this can fail if port already in use!
		acceptor.open(endpoint.protocol());
		acceptor.set_option(asioTCP::socket::reuse_address(true));
		acceptor.bind(endpoint);
		acceptor.listen();

		// If port was zero, we want to update with the actual port number.
		if (config["portNumber"].getValue<dv::ConfigVariant::INTEGER>() == 0) {
			const auto local = acceptor.local_endpoint();
			dvConfigNodePutLong(moduleData->moduleNode, "portNumber", local.port());
		}
	}

	~NetTCPServer() {
		for (const auto client : clients) {
			client->close();
		}

		clients.clear();
	}

	void removeClient(Connection *client) {
		clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
	}

	void run(const libcaer::events::EventPacketContainer &in) {
	}

private:
	void acceptStart() {
		acceptor.async_accept(acceptorNewSocket, [this](const boost::system::error_code &error) {
			if (error) {
				// Ignore cancel error, normal on shutdown.
				if (error != asio::error::operation_aborted) {
					// TODO: error handling.
				}
			}
			else {
				auto client = std::make_shared<Connection>(std::move(acceptorNewSocket), this);

				clients.push_back(client.get());

				client->start();

				acceptStart();
			}
		});
	}
};

Connection::Connection(asioTCP::socket s, NetTCPServer *server) : parent(server), socket(std::move(s), false, nullptr) {
	logger::log(logger::logLevel::INFO, "TCP OUTPUT", "New connection from client %s:%d.",
		socket.remote_address().to_string().c_str(), socket.remote_port());
}

Connection::~Connection() {
	parent->removeClient(this);

	logger::log(logger::logLevel::INFO, "TCP OUTPUT", "Closing connection from client %s:%d.",
		socket.remote_address().to_string().c_str(), socket.remote_port());
}

void Connection::start() {
	auto self(shared_from_this());

	socket.start(
		[this, self](const boost::system::error_code &error) {
			if (error) {
				handleError(error, "Failed startup (TLS handshake)");
			}
			else {
				keepAliveByReading();
			}
		},
		asioSSL::stream_base::server);
}

void Connection::close() {
	socket.close();
}

void Connection::writeBuffer(std::shared_ptr<const asio::const_buffer> buffer) {
	auto self(shared_from_this());

	socket.write(*buffer, [this, self, buffer](const boost::system::error_code &error, size_t /*length*/) {
		if (error) {
			handleError(error, "Failed to write buffer");
		}
	});
}

void Connection::keepAliveByReading() {
	auto self(shared_from_this());

	socket.read(asio::buffer(&keepAliveReadSpace, sizeof(keepAliveReadSpace)),
		[this, self](const boost::system::error_code &error, size_t /*length*/) {
			if (error) {
				handleError(error, "Read keep-alive failure");
			}
			else {
				handleError(error, "Detected illegal incoming data");
			}
		});
}

void Connection::handleError(const boost::system::error_code &error, const char *message) {
	if (error == asio::error::eof) {
		// Handle EOF separately.
		logger::log(logger::logLevel::INFO, "TCP OUTPUT", "Client %s:%d: connection closed.",
			socket.remote_address().to_string().c_str(), socket.remote_port());
	}
	else {
		logger::log(logger::logLevel::ERROR, "TCP OUTPUT", "Client %s:%d: %s. Error: %s (%d).",
			socket.remote_address().to_string().c_str(), socket.remote_port(), message, error.message().c_str(),
			error.value());
	}
}

registerModuleClass(NetTCPServer);
