#ifndef SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_
#define SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_

#include "dv_config_action_data.hpp"

#include <memory>

class ConfigServerConnection;

void ConfigServerHandleRequest(
	std::shared_ptr<ConfigServerConnection> client, std::unique_ptr<uint8_t[]> messageBuffer);

#endif /* SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_ */
