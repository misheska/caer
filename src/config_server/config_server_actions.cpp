#include "config_server_actions.h"

#include "../module.h"
#include "config_server_connection.h"
#include "config_server_main.h"

#include <boost/tokenizer.hpp>
#include <regex>

namespace logger = libcaer::log;

static inline void caerConfigSendError(std::shared_ptr<ConfigServerConnection> client, const std::string &errorMsg) {
	ConfigActionData &response = client->getData();
	response.reset();

	response.setAction(caer_config_actions::CAER_CONFIG_ERROR);
	response.setType(SSHS_STRING);
	response.setNode(errorMsg);

	client->writeResponse();

	logger::log(
		logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Sent error message '%s' back to client.", errorMsg.c_str());
}

static inline void caerConfigSendResponse(std::shared_ptr<ConfigServerConnection> client, caer_config_actions action,
	sshs_node_attr_value_type type, const std::string &message) {
	ConfigActionData &response = client->getData();
	response.reset();

	response.setAction(action);
	response.setType(type);
	response.setNode(message);

	client->writeResponse();

	logger::log(
		logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Sent response back to client: %s.", response.toString().c_str());
}

static inline bool checkNodeExists(
	sshs configStore, const std::string &node, std::shared_ptr<ConfigServerConnection> client) {
	bool nodeExists = sshsExistsNode(configStore, node);

	// Only allow operations on existing nodes, this is for remote
	// control, so we only manipulate what's already there!
	if (!nodeExists) {
		// Send back error message to client.
		caerConfigSendError(client, "Node doesn't exist. Operations are only allowed on existing data.");
	}

	return (nodeExists);
}

static inline bool checkAttributeExists(sshsNode wantedNode, const std::string &key,
	enum sshs_node_attr_value_type type, std::shared_ptr<ConfigServerConnection> client) {
	// Check if attribute exists. Only allow operations on existing attributes!
	bool attrExists = sshsNodeAttributeExists(wantedNode, key, type);

	if (!attrExists) {
		// Send back error message to client.
		caerConfigSendError(
			client, "Attribute of given type doesn't exist. Operations are only allowed on existing data.");
	}

	return (attrExists);
}

static inline void caerConfigSendBoolResponse(
	std::shared_ptr<ConfigServerConnection> client, caer_config_actions action, bool result) {
	// Send back result to client. Format is the same as incoming data.
	caerConfigSendResponse(client, action, SSHS_BOOL, ((result) ? ("true") : ("false")));
}

void caerConfigServerHandleRequest(std::shared_ptr<ConfigServerConnection> client) {
	ConfigActionData &data = client->getData();

	caer_config_actions action     = data.getAction();
	sshs_node_attr_value_type type = data.getType();

	logger::log(
		logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Handling request from client: %s.", data.toString().c_str());

	// Interpretation of data is up to each action individually.
	sshs configStore = sshsGetGlobal();

	switch (action) {
		case caer_config_actions::CAER_CONFIG_NODE_EXISTS: {
			// We only need the node name here. Type is not used (ignored)!
			bool result = sshsExistsNode(configStore, data.getNode());

			// Send back result to client. Format is the same as incoming data.
			caerConfigSendBoolResponse(client, caer_config_actions::CAER_CONFIG_NODE_EXISTS, result);

			break;
		}

		case caer_config_actions::CAER_CONFIG_ATTR_EXISTS: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, data.getNode());

			// Check if attribute exists.
			bool result = sshsNodeAttributeExists(wantedNode, data.getKey(), type);

			// Send back result to client. Format is the same as incoming data.
			caerConfigSendBoolResponse(client, caer_config_actions::CAER_CONFIG_ATTR_EXISTS, result);

			break;
		}

		case caer_config_actions::CAER_CONFIG_GET: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			union sshs_node_attr_value result = sshsNodeGetAttribute(wantedNode, data.getKey().c_str(), type);

			char *resultStr = sshsHelperValueToStringConverter(type, result);

			if (resultStr == nullptr) {
				// Send back error message to client.
				caerConfigSendError(client, "Failed to allocate memory for value string.");
			}
			else {
				caerConfigSendResponse(client, caer_config_actions::CAER_CONFIG_GET, type, resultStr);

				free(resultStr);
			}

			// If this is a string, we must remember to free the original result.str
			// too, since it will also be a copy of the string coming from SSHS.
			if (type == SSHS_STRING) {
				free(result.string);
			}

			break;
		}

		case caer_config_actions::CAER_CONFIG_PUT: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			// Put given value into config node. Node, attr and type are already verified.
			const char *typeStr = sshsHelperTypeToStringConverter(type);
			if (!sshsNodeStringToAttributeConverter(
					wantedNode, data.getKey().c_str(), typeStr, data.getValue().c_str())) {
				// Send back correct error message to client.
				if (errno == EINVAL) {
					caerConfigSendError(client, "Impossible to convert value according to type.");
				}
				else if (errno == EPERM) {
					caerConfigSendError(client, "Cannot write to a read-only attribute.");
				}
				else if (errno == ERANGE) {
					caerConfigSendError(client, "Value out of attribute range.");
				}
				else {
					// Unknown error.
					caerConfigSendError(client, "Unknown error.");
				}

				break;
			}

			// Send back confirmation to the client.
			caerConfigSendBoolResponse(client, caer_config_actions::CAER_CONFIG_PUT, true);

			break;
		}

		case caer_config_actions::CAER_CONFIG_GET_CHILDREN: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, data.getNode());

			// Get the names of all the child nodes and return them.
			size_t numNames;
			const char **childNames = sshsNodeGetChildNames(wantedNode, &numNames);

			// No children at all, return empty.
			if (childNames == nullptr) {
				// Send back error message to client.
				caerConfigSendError(client, "Node has no children.");

				break;
			}

			// We need to return a big string with all of the child names,
			// separated by a | character.
			size_t namesLength = 0;

			for (size_t i = 0; i < numNames; i++) {
				namesLength += strlen(childNames[i]) + 1; // +1 for | separator.
			}

			// Allocate a buffer for the names and copy them over.
			char namesBuffer[namesLength];

			for (size_t i = 0, acc = 0; i < numNames; i++) {
				size_t len = strlen(childNames[i]);

				memcpy(namesBuffer + acc, childNames[i], len);
				acc += len;

				namesBuffer[acc++] = '|';
			}

			free(childNames);

			caerConfigSendResponse(client, caer_config_actions::CAER_CONFIG_GET_CHILDREN, SSHS_STRING,
				std::string(namesBuffer, namesLength - 1));

			break;
		}

		case caer_config_actions::CAER_CONFIG_GET_ATTRIBUTES: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, data.getNode());

			// Get the keys of all the attributes and return them.
			size_t numKeys;
			const char **attrKeys = sshsNodeGetAttributeKeys(wantedNode, &numKeys);

			// No attributes at all, return empty.
			if (attrKeys == nullptr) {
				// Send back error message to client.
				caerConfigSendError(client, "Node has no attributes.");

				break;
			}

			// We need to return a big string with all of the attribute keys,
			// separated by a | character.
			size_t keysLength = 0;

			for (size_t i = 0; i < numKeys; i++) {
				keysLength += strlen(attrKeys[i]) + 1; // +1 for | separator.
			}

			// Allocate a buffer for the keys and copy them over.
			char keysBuffer[keysLength];

			for (size_t i = 0, acc = 0; i < numKeys; i++) {
				size_t len = strlen(attrKeys[i]);

				memcpy(keysBuffer + acc, attrKeys[i], len);
				acc += len;

				keysBuffer[acc++] = '|';
			}

			free(attrKeys);

			caerConfigSendResponse(client, caer_config_actions::CAER_CONFIG_GET_ATTRIBUTES, SSHS_STRING,
				std::string(keysBuffer, keysLength - 1));

			break;
		}

		case caer_config_actions::CAER_CONFIG_GET_TYPE: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, data.getNode());

			// Check if any keys match the given one and return its type.
			enum sshs_node_attr_value_type attrType = sshsNodeGetAttributeType(wantedNode, data.getKey().c_str());

			// No attributes for specified key, return empty.
			if (attrType == SSHS_UNKNOWN) {
				// Send back error message to client.
				caerConfigSendError(client, "Node has no attributes with specified key.");

				break;
			}

			// We need to return a string with the attribute type,
			// separated by a NUL character.
			const char *typeStr = sshsHelperTypeToStringConverter(attrType);

			caerConfigSendResponse(client, caer_config_actions::CAER_CONFIG_GET_TYPE, SSHS_STRING, typeStr);

			break;
		}

		case caer_config_actions::CAER_CONFIG_GET_RANGES: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			struct sshs_node_attr_ranges ranges = sshsNodeGetAttributeRanges(wantedNode, data.getKey().c_str(), type);

			// We need to return a string with the two ranges,
			// separated by a | character.
			char buf[256];
			size_t bufLen = 0;

			switch (type) {
				case SSHS_UNKNOWN:
				case SSHS_BOOL:
					bufLen = 3;
					memcpy(buf, "0|0", bufLen);
					break;

				case SSHS_INT:
					bufLen = (size_t) snprintf(
						buf, 256, "%" PRIi32 "|%" PRIi32, ranges.min.iintRange, ranges.max.iintRange);
					break;

				case SSHS_LONG:
					bufLen = (size_t) snprintf(
						buf, 256, "%" PRIi64 "|%" PRIi64, ranges.min.ilongRange, ranges.max.ilongRange);
					break;

				case SSHS_FLOAT:
					bufLen = (size_t) snprintf(
						buf, 256, "%g|%g", (double) ranges.min.ffloatRange, (double) ranges.max.ffloatRange);
					break;

				case SSHS_DOUBLE:
					bufLen = (size_t) snprintf(buf, 256, "%g|%g", ranges.min.ddoubleRange, ranges.max.ddoubleRange);
					break;

				case SSHS_STRING:
					bufLen = (size_t) snprintf(buf, 256, "%zu|%zu", ranges.min.stringRange, ranges.max.stringRange);
					break;
			}

			caerConfigSendResponse(client, caer_config_actions::CAER_CONFIG_GET_RANGES, type, std::string(buf, bufLen));

			break;
		}

		case caer_config_actions::CAER_CONFIG_GET_FLAGS: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			int flags = sshsNodeGetAttributeFlags(wantedNode, data.getKey().c_str(), type);

			std::string flagsStr;

			if (flags & SSHS_FLAGS_READ_ONLY) {
				flagsStr = "READ_ONLY";
			}
			else if (flags & SSHS_FLAGS_NOTIFY_ONLY) {
				flagsStr = "NOTIFY_ONLY";
			}
			else {
				flagsStr = "NORMAL";
			}

			if (flags & SSHS_FLAGS_NO_EXPORT) {
				flagsStr += ",NO_EXPORT";
			}

			caerConfigSendResponse(client, caer_config_actions::CAER_CONFIG_GET_FLAGS, SSHS_STRING, flagsStr);

			break;
		}

		case caer_config_actions::CAER_CONFIG_GET_DESCRIPTION: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			char *description = sshsNodeGetAttributeDescription(wantedNode, data.getKey().c_str(), type);

			caerConfigSendResponse(client, caer_config_actions::CAER_CONFIG_GET_DESCRIPTION, SSHS_STRING, description);

			free(description);

			break;
		}

		case caer_config_actions::CAER_CONFIG_ADD_MODULE: {
			if (data.getNode().length() == 0) {
				// Disallow empty strings.
				caerConfigSendError(client, "Name cannot be empty.");
				break;
			}

			if (data.getKey().length() == 0) {
				// Disallow empty strings.
				caerConfigSendError(client, "Library cannot be empty.");
				break;
			}

			// Node is the module name, key the library.
			const std::string moduleName(data.getNode());
			const std::string moduleLibrary(data.getKey());

			// Check module name.
			if (moduleName == "caer") {
				caerConfigSendError(client, "Name is reserved for system use.");
				break;
			}

			const std::regex moduleNameRegex("^[a-zA-Z-_\\d\\.]+$");

			if (!std::regex_match(moduleName, moduleNameRegex)) {
				caerConfigSendError(client, "Name uses invalid characters.");
				break;
			}

			if (sshsExistsNode(configStore, "/" + moduleName + "/")) {
				caerConfigSendError(client, "Name is already in use.");
				break;
			}

			// Check module library.
			sshsNode modulesSysNode              = sshsGetNode(configStore, "/caer/modules/");
			const std::string modulesListOptions = sshsNodeGetStdString(modulesSysNode, "modulesListOptions");

			boost::tokenizer<boost::char_separator<char>> modulesList(
				modulesListOptions, boost::char_separator<char>(","));

			if (!findBool(modulesList.begin(), modulesList.end(), moduleLibrary)) {
				caerConfigSendError(client, "Library does not exist.");
				break;
			}

			// Name and library are fine, let's determine the next free ID.
			size_t rootNodesSize;
			sshsNode *rootNodes = sshsNodeGetChildren(sshsGetNode(configStore, "/"), &rootNodesSize);

			std::vector<int16_t> usedModuleIDs;

			for (size_t i = 0; i < rootNodesSize; i++) {
				sshsNode mNode = rootNodes[i];

				if (!sshsNodeAttributeExists(mNode, "moduleId", SSHS_INT)) {
					continue;
				}

				int16_t moduleID = I16T(sshsNodeGetInt(mNode, "moduleId"));
				usedModuleIDs.push_back(moduleID);
			}

			free(rootNodes);

			vectorSortUnique(usedModuleIDs);

			size_t idx               = 0;
			int16_t nextFreeModuleID = 1;

			while (idx < usedModuleIDs.size() && usedModuleIDs[idx] == nextFreeModuleID) {
				idx++;
				nextFreeModuleID++;
			}

			// Let's create the needed configuration for the new module.
			sshsNode newModuleNode = sshsGetNode(configStore, "/" + moduleName + "/");

			sshsNodeCreate(newModuleNode, "moduleId", nextFreeModuleID, I16T(1), I16T(INT16_MAX), SSHS_FLAGS_READ_ONLY,
				"Module ID.");
			sshsNodeCreate(
				newModuleNode, "moduleLibrary", moduleLibrary, 1, PATH_MAX, SSHS_FLAGS_READ_ONLY, "Module library.");

			// Add moduleInput/moduleOutput as appropriate.
			sshsNode moduleSysNode = sshsGetRelativeNode(modulesSysNode, moduleLibrary + "/");
			std::string inputType  = sshsNodeGetStdString(moduleSysNode, "type");

			if (inputType != "INPUT") {
				// CAER_MODULE_OUTPUT / CAER_MODULE_PROCESSOR
				// moduleInput must exist for OUTPUT and PROCESSOR modules.
				sshsNodeCreate(
					newModuleNode, "moduleInput", "", 0, 1024, SSHS_FLAGS_NORMAL, "Module dynamic input definition.");
			}

			if (inputType != "OUTPUT") {
				// CAER_MODULE_INPUT / CAER_MODULE_PROCESSOR
				// moduleOutput must exist for INPUT and PROCESSOR modules, only
				// if their outputs are undefined (-1).
				if (sshsExistsRelativeNode(moduleSysNode, "outputStreams/0/")) {
					sshsNode outputNode0    = sshsGetRelativeNode(moduleSysNode, "outputStreams/0/");
					int32_t outputNode0Type = sshsNodeGetInt(outputNode0, "type");

					if (outputNode0Type == -1) {
						sshsNodeCreate(newModuleNode, "moduleOutput", "", 0, 1024, SSHS_FLAGS_NORMAL,
							"Module dynamic output definition.");
					}
				}
			}

			// Create static module configuration, so users can start
			// changing it right away after module add.
			caerModuleConfigInit(newModuleNode);

			// Send back confirmation to the client.
			caerConfigSendBoolResponse(client, caer_config_actions::CAER_CONFIG_ADD_MODULE, true);

			break;
		}

		case caer_config_actions::CAER_CONFIG_REMOVE_MODULE: {
			if (data.getNode().length() == 0) {
				// Disallow empty strings.
				caerConfigSendError(client, "Name cannot be empty.");
				break;
			}

			// Node is the module name.
			const std::string moduleName(data.getNode());

			// Check module name.
			if (moduleName == "caer") {
				caerConfigSendError(client, "Name is reserved for system use.");
				break;
			}

			if (!sshsExistsNode(configStore, "/" + moduleName + "/")) {
				caerConfigSendError(client, "Name is not in use.");
				break;
			}

			// Modules can only be deleted while the mainloop is not running, to not
			// destroy data the system is relying on.
			bool isMainloopRunning = sshsNodeGetBool(sshsGetNode(configStore, "/"), "running");
			if (isMainloopRunning) {
				caerConfigSendError(client, "Mainloop is running.");
				break;
			}

			// Truly delete the node and all its children.
			sshsNodeRemoveNode(sshsGetNode(configStore, "/" + moduleName + "/"));

			// Send back confirmation to the client.
			caerConfigSendBoolResponse(client, caer_config_actions::CAER_CONFIG_REMOVE_MODULE, true);

			break;
		}

		case caer_config_actions::CAER_CONFIG_ADD_PUSH_CLIENT: {
			client->addPushClient();

			// Send back confirmation to the client.
			caerConfigSendBoolResponse(client, caer_config_actions::CAER_CONFIG_ADD_PUSH_CLIENT, true);

			break;
		}

		case caer_config_actions::CAER_CONFIG_REMOVE_PUSH_CLIENT: {
			client->removePushClient();

			// Send back confirmation to the client.
			caerConfigSendBoolResponse(client, caer_config_actions::CAER_CONFIG_REMOVE_PUSH_CLIENT, true);

			break;
		}

		default: {
			// Unknown action, send error back to client.
			caerConfigSendError(client, "Unknown action.");

			break;
		}
	}
}
