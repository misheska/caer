#ifndef SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_
#define SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_

#include "config_action_data.h"

#include <memory>

class ConfigServerConnection;

void caerConfigServerHandleRequest(std::shared_ptr<ConfigServerConnection> client);

#endif /* SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_ */
