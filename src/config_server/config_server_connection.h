#ifndef SRC_CONFIG_SERVER_CONFIG_SERVER_CONNECTION_H_
#define SRC_CONFIG_SERVER_CONFIG_SERVER_CONNECTION_H_

#include "asio.h"
#include "config_action_data.h"

#include <memory>

class ConfigServer;

class ConfigServerConnection : public std::enable_shared_from_this<ConfigServerConnection> {
private:
	ConfigServer *parent;
	TCPTLSWriteOrderedSocket socket;
	ConfigActionData data;

public:
	ConfigServerConnection(asioTCP::socket s, bool sslEnabled, asioSSL::context &sslContext, ConfigServer *server);
	~ConfigServerConnection();

	void start();
	void close();

	void addPushClient();
	void removePushClient();

	ConfigActionData &getData();
	void writeResponse();
	void writePushMessage(std::shared_ptr<const ConfigActionData> message);

private:
	void readHeader();
	void readData();
	void handleError(const boost::system::error_code &error, const char *message);
};

#endif /* SRC_CONFIG_SERVER_CONFIG_SERVER_CONNECTION_H_ */
