#include "dv-sdk/config/dvConfig.hpp"
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
	dvOutput output;

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

		acceptor.open(endpoint.protocol());
		acceptor.set_option(asioTCP::socket::reuse_address(true));
		acceptor.bind(endpoint);
		acceptor.listen();

		// If port was zero, we want to update with the actual port number.
		if (config["portNumber"].getValue<dv::ConfigVariant::INTEGER>() == 0) {
			const auto local = acceptor.local_endpoint();
			dvConfigNodePutLong(moduleData->moduleNode, "portNumber", local.port());
		}

		logger::log(logger::logLevel::INFO, "TCP OUTPUT", "Output server ready on %s:%d.",
			config["ipAddress"].getValue<dv::ConfigVariant::STRING>().c_str(),
			config["portNumber"].getValue<dv::ConfigVariant::INTEGER>());

		makeSourceInfo(moduleData->moduleNode);

		acceptStart();
	}

	~NetTCPServer() {
		acceptor.close();

		for (const auto client : clients) {
			client->close();
		}

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

	void run(const libcaer::events::EventPacketContainer &in) {
		if (!in.empty()) {
			for (const auto &pkt : in) {
				struct arraydef inData;

				if (pkt->getEventType() == POLARITY_EVENT) {
					inData.typeId = *(reinterpret_cast<const uint32_t *>(PolarityPacketIdentifier()));
					inData.size   = static_cast<size_t>(pkt->getEventValid());
					inData.ptr    = convertToAedat4(POLARITY_EVENT, pkt.get());
				}
				else if (pkt->getEventType() == FRAME_EVENT) {
					inData.typeId = *(reinterpret_cast<const uint32_t *>(Frame8PacketIdentifier()));
					inData.size   = static_cast<size_t>(pkt->getEventValid());
					inData.ptr    = convertToAedat4(FRAME_EVENT, pkt.get());
				}
				else {
					// Skip unknown packet.
					continue;
				}

				// outData.size is in bytes, not elements.
				auto outData = output.processPacket(inData);

				auto outBuffer = std::make_shared<asio::const_buffer>(outData.ptr, outData.size);

				for (const auto client : clients) {
					client->writeBuffer(outBuffer);
				}
			}
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
		acceptor.async_accept(acceptorNewSocket, [this](const boost::system::error_code &error) {
			if (error) {
				// Ignore cancel error, normal on shutdown.
				if (error != asio::error::operation_aborted) {
					logger::log(logger::logLevel::ERROR, "TCP OUTPUT", "Failed to accept connection. Error: %s (%d).",
						error.message().c_str(), error.value());
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

	// TODO: this is all manual for now.
	void makeSourceInfo(dv::Config::Node moduleNode) {
		auto sourceInfoNode = moduleNode.getRelativeNode("sourceInfo/");

		// First stream for now.
		auto streamInfoNode = sourceInfoNode.getRelativeNode("0/");

		if (moduleNode.getName() == "_visualizer_event") {
			streamInfoNode.create<dv::Config::AttributeType::STRING>("type", "POLA", {0, UINT16_MAX},
				dv::Config::AttributeFlags::READ_ONLY | dv::Config::AttributeFlags::NO_EXPORT, "Type of data.");
		}
		else if (moduleNode.getName() == "_visualizer_frame") {
			streamInfoNode.create<dv::Config::AttributeType::STRING>("type", "FRM8", {0, UINT16_MAX},
				dv::Config::AttributeFlags::READ_ONLY | dv::Config::AttributeFlags::NO_EXPORT, "Type of data.");
		}
		else {
			streamInfoNode.create<dv::Config::AttributeType::STRING>("type", "UNKN", {0, UINT16_MAX},
				dv::Config::AttributeFlags::READ_ONLY | dv::Config::AttributeFlags::NO_EXPORT, "Type of data.");
		}

		// Fixed at 346x260 for now.
		streamInfoNode.create<dv::Config::AttributeType::INT>("width", 346, {0, UINT16_MAX},
			dv::Config::AttributeFlags::READ_ONLY | dv::Config::AttributeFlags::NO_EXPORT, "Data width.");
		streamInfoNode.create<dv::Config::AttributeType::INT>("height", 260, {0, UINT16_MAX},
			dv::Config::AttributeFlags::READ_ONLY | dv::Config::AttributeFlags::NO_EXPORT, "Data height.");
	}

	static void *convertToAedat4(int16_t type, const libcaer::events::EventPacket *oldPacket) {
		switch (type) {
			case POLARITY_EVENT: {
				auto newPacket = new PolarityPacketT();

				auto oldPacketPolarity = dynamic_cast<const libcaer::events::PolarityEventPacket *>(oldPacket);

				for (const auto &evt : *oldPacketPolarity) {
					if (!evt.isValid()) {
						continue;
					}

					newPacket->events.emplace_back(
						evt.getTimestamp64(*oldPacketPolarity), evt.getX(), evt.getY(), evt.getPolarity());
				}

				return (newPacket);
				break;
			}

			case FRAME_EVENT: {
				auto newPacket = new Frame8PacketT();

				auto oldPacketFrame = dynamic_cast<const libcaer::events::FrameEventPacket *>(oldPacket);

				for (const auto &evt : *oldPacketFrame) {
					if (!evt.isValid()) {
						continue;
					}

					Frame8T newFrame{};

					newFrame.timestamp                = evt.getTimestamp64(*oldPacketFrame);
					newFrame.timestampStartOfFrame    = evt.getTSStartOfFrame64(*oldPacketFrame);
					newFrame.timestampStartOfExposure = evt.getTSStartOfExposure64(*oldPacketFrame);
					newFrame.timestampEndOfExposure   = evt.getTSEndOfExposure64(*oldPacketFrame);
					newFrame.timestampEndOfFrame      = evt.getTSEndOfFrame64(*oldPacketFrame);

					newFrame.origColorFilter = static_cast<FrameColorFilters>(evt.getColorFilter());
					newFrame.numChannels     = static_cast<FrameChannels>(evt.getChannelNumber());

					newFrame.lengthX   = static_cast<int16_t>(evt.getLengthX());
					newFrame.lengthY   = static_cast<int16_t>(evt.getLengthY());
					newFrame.positionX = static_cast<int16_t>(evt.getPositionX());
					newFrame.positionY = static_cast<int16_t>(evt.getPositionY());

					for (size_t px = 0; px < evt.getPixelsMaxIndex(); px++) {
						newFrame.pixels.push_back(static_cast<uint8_t>(evt.getPixelArrayUnsafe()[px] >> 8));
					}

					newPacket->events.emplace_back(newFrame);
				}

				return (newPacket);
				break;
			}

			default:
				return (nullptr);
				break;
		}
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

registerModuleClass(NetTCPServer)
