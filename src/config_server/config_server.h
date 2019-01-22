#ifndef SRC_CONFIG_SERVER_CONFIG_SERVER_H_
#define SRC_CONFIG_SERVER_CONFIG_SERVER_H_

#include "asio.h"
#include "config_server_connection.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

class ConfigServer {
private:
	enum class IOThreadState { STARTING, RUNNING, STOPPING, STOPPED };

	static thread_local uint64_t currentClientID;

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
	std::atomic_size_t numPushClients;

public:
	ConfigServer();

	void threadStart();
	void serviceRestart();
	void threadStop();

	void setCurrentClientID(uint64_t clientID);
	void removeClient(ConfigServerConnection *client);
	void addPushClient(ConfigServerConnection *pushClient);
	void removePushClient(ConfigServerConnection *pushClient);
	bool pushClientsPresent();
	void pushMessageToClients(std::shared_ptr<caerConfigActionData> message);

private:
	void serviceConfigure();
	void serviceStart();
	void serviceStop();
	void acceptStart();
};

#endif /* SRC_CONFIG_SERVER_CONFIG_SERVER_H_ */
