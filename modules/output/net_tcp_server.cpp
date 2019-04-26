#define DV_API_OPENCV_SUPPORT 0
#include "dv-sdk/module.hpp"

#include "dv_output.hpp"
#include "src/config_server/asio.h"

class NetTCPServer;

class Connection : public std::enable_shared_from_this<Connection> {
private:
	NetTCPServer *parent;
	TCPTLSWriteOrderedSocket socket;
	uint8_t keepAliveReadSpace;

public:
	Connection(asioTCP::socket s, bool tlsEnabled, asioSSL::context *tlsContext, NetTCPServer *server);
	~Connection();

	void start();
	void close();
	void writeMessage(std::shared_ptr<const flatbuffers::FlatBufferBuilder> message);

private:
	void keepAliveByReading();
	void handleError(const boost::system::error_code &error, const char *message);
};

class NetTCPServer : public dv::ModuleBase {
private:
	asio::io_service ioService;
	asioTCP::acceptor acceptor;
	asioTCP::socket acceptorNewSocket;
	asioSSL::context tlsContext;
	bool tlsEnabled;

	std::vector<Connection *> clients;
	dvOutput output;

public:
	static void addInputs(std::vector<dv::InputDefinition> &in) {
		in.emplace_back("output0", "ANYT", false);
	}

	static const char *getDescription() {
		return ("Send AEDAT 4 data out via TCP to connected clients (server mode).");
	}

	static void getConfigOptions(dv::RuntimeConfig &config) {
		config.add(
			"ipAddress", dv::ConfigOption::stringOption("IPv4 address to listen on (server mode).", "127.0.0.1"));
		config.add(
			"portNumber", dv::ConfigOption::intOption("Port number to listen on (server mode).", 7777, 0, UINT16_MAX));
		config.add("backlogSize", dv::ConfigOption::intOption("Maximum number of pending connections.", 5, 1, 32));
		config.add("concurrentConnections",
			dv::ConfigOption::intOption("Maximum number of concurrent active connections.", 10, 1, 128));
	}

	NetTCPServer() :
		acceptor(ioService),
		acceptorNewSocket(ioService),
		tlsContext(asioSSL::context::tlsv12_server),
		tlsEnabled(false) {
		// First check that input is possible.
		auto inputInfoNode = inputs.getUntypedInfo("output0");
		if (!inputInfoNode) {
			throw std::runtime_error("Input not ready, upstream module not running.");
		}

		auto inputNode = inputInfoNode.getParent();

		auto outputNode     = moduleNode.getRelativeNode("outputs/output0/");
		auto outputInfoNode = outputNode.getRelativeNode("info/");

		inputNode.copyTo(outputNode);
		inputInfoNode.copyTo(outputInfoNode);

		// Configure acceptor.
		auto endpoint = asioTCP::endpoint(asioIP::address::from_string(config.get<dv::CfgType::STRING>("ipAddress")),
			static_cast<uint16_t>(config.get<dv::CfgType::INT>("portNumber")));

		acceptor.open(endpoint.protocol());
		acceptor.set_option(asioTCP::socket::reuse_address(true));
		acceptor.bind(endpoint);
		acceptor.listen();

		// If port was zero, we want to update with the actual port number.
		if (config.get<dv::CfgType::INT>("portNumber") == 0) {
			const auto local = acceptor.local_endpoint();
			config.set<dv::CfgType::INT>("portNumber", local.port());
		}

		acceptStart();

		log.info << boost::format("Output server ready on %s:%d.") % config.get<dv::CfgType::STRING>("ipAddress")
						% config.get<dv::CfgType::INT>("portNumber");
	}

	~NetTCPServer() override {
		acceptor.close();

		// Post 'close all connections' to end of async queue,
		// so that any other callbacks, such as pending accepts,
		// are executed first, and we really close all sockets.
		ioService.post([this]() {
			// Close all open connections, hard.
			for (const auto client : clients) {
				client->close();
			}
		});

		// Wait for all clients to go away.
		while (!clients.empty()) {
			ioService.poll();
#if defined(BOOST_VERSION) && (BOOST_VERSION / 100000) == 1 && (BOOST_VERSION / 100 % 1000) >= 66
			ioService.restart();
#else
			ioService.reset();
#endif
		}
	}

	void removeClient(Connection *client) {
		clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
	}

	void run() override {
		auto input0 = dvModuleInputGet(moduleData, "output0");

		if (input0 != nullptr) {
			// outData.size is in bytes, not elements.
			auto outMessage = output.processPacket(input0);

			for (const auto client : clients) {
				client->writeMessage(outMessage);
			}

			dvModuleInputDismiss(moduleData, "output0", input0);
		}

		ioService.poll();
#if defined(BOOST_VERSION) && (BOOST_VERSION / 100000) == 1 && (BOOST_VERSION / 100 % 1000) >= 66
		ioService.restart();
#else
		ioService.reset();
#endif
	}

private:
	void acceptStart() {
		acceptor.async_accept(
			acceptorNewSocket,
			[this](const boost::system::error_code &error) {
				if (error) {
					// Ignore cancel error, normal on shutdown.
					if (error != asio::error::operation_aborted) {
						log.error << boost::format("Failed to accept connection. Error: %s (%d).") % error.message()
										 % error.value();
					}
				}
				else {
					auto client
						= std::make_shared<Connection>(std::move(acceptorNewSocket), tlsEnabled, &tlsContext, this);

					clients.push_back(client.get());

					client->start();

					acceptStart();
				}
			},
			nullptr);
	}
};

Connection::Connection(asioTCP::socket s, bool tlsEnabled, asioSSL::context *tlsContext, NetTCPServer *server) :
	parent(server),
	socket(std::move(s), tlsEnabled, tlsContext),
	keepAliveReadSpace(0) {
	parent->log.info << boost::format("New connection from client %s:%d.") % socket.remote_address().to_string()
							% socket.remote_port();
}

Connection::~Connection() {
	parent->removeClient(this);

	parent->log.info << boost::format("Closing connection from client %s:%d.") % socket.remote_address().to_string()
							% socket.remote_port();
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

void Connection::writeMessage(std::shared_ptr<const flatbuffers::FlatBufferBuilder> message) {
	auto self(shared_from_this());

	socket.write(asio::buffer(message->GetBufferPointer(), message->GetSize()),
		[this, self, message](const boost::system::error_code &error, size_t /*length*/) {
			if (error) {
				handleError(error, "Failed to write message");
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
		parent->log.info << boost::format("Client %s:%d: connection closed.") % socket.remote_address().to_string()
								% socket.remote_port();
	}
	else {
		parent->log.error << boost::format("Client %s:%d: %s. Error: %s (%d).") % socket.remote_address().to_string()
								 % socket.remote_port() % message % error.message() % error.value();
	}
}

registerModuleClass(NetTCPServer)
