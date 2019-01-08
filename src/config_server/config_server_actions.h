#ifndef SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_
#define SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_

#include "config_server_main.h"

#include <memory>

enum class caer_config_actions {
	CAER_CONFIG_NODE_EXISTS        = 0,
	CAER_CONFIG_ATTR_EXISTS        = 1,
	CAER_CONFIG_GET                = 2,
	CAER_CONFIG_PUT                = 3,
	CAER_CONFIG_ERROR              = 4,
	CAER_CONFIG_GET_CHILDREN       = 5,
	CAER_CONFIG_GET_ATTRIBUTES     = 6,
	CAER_CONFIG_GET_TYPE           = 7,
	CAER_CONFIG_GET_RANGES         = 8,
	CAER_CONFIG_GET_FLAGS          = 9,
	CAER_CONFIG_GET_DESCRIPTION    = 10,
	CAER_CONFIG_ADD_MODULE         = 11,
	CAER_CONFIG_REMOVE_MODULE      = 12,
	CAER_CONFIG_ADD_PUSH_CLIENT    = 13,
	CAER_CONFIG_REMOVE_PUSH_CLIENT = 14,
	CAER_CONFIG_PUSH_MESSAGE       = 15,
};

class ConfigServerConnection;

void caerConfigServerHandleRequest(std::shared_ptr<ConfigServerConnection> client);

#endif /* SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_ */
