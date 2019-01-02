#ifndef SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_
#define SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_

#include "config_server_connection.h"
#include "config_server_main.h"

#include <memory>

enum caer_config_actions {
	CAER_CONFIG_NODE_EXISTS         = 0,
	CAER_CONFIG_ATTR_EXISTS         = 1,
	CAER_CONFIG_GET                 = 2,
	CAER_CONFIG_PUT                 = 3,
	CAER_CONFIG_ERROR               = 4,
	CAER_CONFIG_GET_CHILDREN        = 5,
	CAER_CONFIG_GET_ATTRIBUTES      = 6,
	CAER_CONFIG_GET_TYPE            = 7,
	CAER_CONFIG_GET_RANGES          = 8,
	CAER_CONFIG_GET_FLAGS           = 9,
	CAER_CONFIG_GET_DESCRIPTION     = 10,
	CAER_CONFIG_ADD_MODULE          = 11,
	CAER_CONFIG_REMOVE_MODULE       = 12,
	CAER_CONFIG_ADD_TO_PUSH_CLIENTS = 13,
};

void caerConfigServerHandleRequest(std::shared_ptr<ConfigServerConnection> client, uint8_t action, uint8_t type,
	const uint8_t *extra, size_t extraLength, const uint8_t *node, size_t nodeLength, const uint8_t *key,
	size_t keyLength, const uint8_t *value, size_t valueLength);

#endif /* SRC_CONFIG_SERVER_CONFIG_SERVER_ACTIONS_H_ */
