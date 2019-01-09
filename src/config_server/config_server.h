#ifndef SRC_CONFIG_SERVER_CONFIG_SERVER_H_
#define SRC_CONFIG_SERVER_CONFIG_SERVER_H_

#include "asio.h"
#include "config_server_connection.h"

#include <memory>
#include <thread>
#include <vector>

class ConfigServer {
private:
	enum class IOThreadState { STARTING, RUNNING, STOPPING, STOPPED };

	bool ioThreadRun;
	IOThreadState ioThreadState;
	std::thread ioThread;
	asio::io_service ioService;
	asioTCP::acceptor acceptor;
	asioTCP::socket acceptorNewSocket;

	asioSSL::context sslContext;
	bool sslEnabled;

	std::vector<ConfigServerConnection *> clients;
	std::vector<ConfigServerConnection *> pushClients;

public:
	ConfigServer();

	void threadStart();
	void serviceRestart();
	void threadStop();

	void removeClient(ConfigServerConnection *client);
	void addPushClient(ConfigServerConnection *pushClient);
	void removePushClient(ConfigServerConnection *pushClient);

private:
	void serviceConfigure();
	void serviceStart();
	void serviceStop();
	void acceptStart();
};

#endif /* SRC_CONFIG_SERVER_CONFIG_SERVER_H_ */
