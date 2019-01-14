#ifndef SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_
#define SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_

#include <memory>
#include "caer_config_action_data.h"

class ConfigServerConnection;

void caerConfigServerHandleRequest(std::shared_ptr<ConfigServerConnection> client);

#endif /* SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_ */
