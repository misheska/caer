#ifndef SRC_CONFIG_SERVER_CONFIG_SERVER_CONNECTION_H_
#define SRC_CONFIG_SERVER_CONFIG_SERVER_CONNECTION_H_

#include "asio.h"
#include "caer_config_action_data.h"

#include <memory>

class ConfigServer;

class ConfigServerConnection : public std::enable_shared_from_this<ConfigServerConnection> {
private:
	static std::atomic_uint64_t clientIDGenerator;

	ConfigServer *parent;
	TCPTLSWriteOrderedSocket socket;
	caerConfigActionData data;
	uint64_t clientID;

public:
	ConfigServerConnection(asioTCP::socket s, bool sslEnabled, asioSSL::context *sslContext, ConfigServer *server);
	~ConfigServerConnection();

	void start();
	void close();

	uint64_t getClientID();

	void addPushClient();
	void removePushClient();

	caerConfigActionData &getData();
	void writeResponse();
	void writePushMessage(std::shared_ptr<const caerConfigActionData> message);

private:
	void readHeader();
	void readData();
	void handleError(const boost::system::error_code &error, const char *message);
};

#endif /* SRC_CONFIG_SERVER_CONFIG_SERVER_CONNECTION_H_ */
