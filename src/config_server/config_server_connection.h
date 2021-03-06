#ifndef SRC_CONFIG_SERVER_CONFIG_SERVER_CONNECTION_H_
#define SRC_CONFIG_SERVER_CONFIG_SERVER_CONNECTION_H_

#include "dv-sdk/cross/asio_tcptlssocket.hpp"

#include "dv_config_action_data.hpp"

#include <memory>

#define DV_CONFIG_SERVER_MAX_INCOMING_SIZE (8 * 1024)

class ConfigServer;

class ConfigServerConnection : public std::enable_shared_from_this<ConfigServerConnection> {
private:
	static std::atomic_uint64_t clientIDGenerator;

	ConfigServer *parent;
	TCPTLSWriteOrderedSocket socket;
	uint64_t clientID;
	flatbuffers::uoffset_t incomingMessageSize;

public:
	ConfigServerConnection(asioTCP::socket s, bool tlsEnabled, asioSSL::context *tlsContext, ConfigServer *server);
	~ConfigServerConnection();

	void start();
	void close();

	uint64_t getClientID();

	void addPushClient();
	void removePushClient();

	void writeMessage(std::shared_ptr<const flatbuffers::FlatBufferBuilder> message);
	void writePushMessage(std::shared_ptr<const flatbuffers::FlatBufferBuilder> message);

private:
	void readMessageSize();
	void readMessage();
	void handleError(const boost::system::error_code &error, const char *message);
};

#endif /* SRC_CONFIG_SERVER_CONFIG_SERVER_CONNECTION_H_ */
