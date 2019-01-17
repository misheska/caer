#include "config_server_actions.h"

#include "caer-sdk/cross/portable_io.h"

#include "../module.h"
#include "config_server_connection.h"
#include "config_server_main.h"

#include <boost/tokenizer.hpp>
#include <regex>

namespace logger = libcaer::log;
namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

static inline void caerConfigSendError(std::shared_ptr<ConfigServerConnection> client, const std::string &errorMsg) {
	caerConfigActionData &response = client->getData();
	response.reset();

	response.setAction(caerConfigAction::ERROR);
	response.setType(SSHS_STRING);
	response.setNode(errorMsg);

	client->writeResponse();

	logger::log(
		logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Sent error message '%s' back to client.", errorMsg.c_str());
}

static inline void caerConfigSendResponse(std::shared_ptr<ConfigServerConnection> client, caerConfigAction action,
	dvCfgType type, const std::string &message) {
	caerConfigActionData &response = client->getData();
	response.reset();

	response.setAction(action);
	response.setType(static_cast<enum sshs_node_attr_value_type>(type));
	response.setNode(message);

	client->writeResponse();

	logger::log(
		logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Sent response back to client: %s.", response.toString().c_str());
}

static inline bool checkNodeExists(
	dvCfg::Tree configStore, const std::string &node, std::shared_ptr<ConfigServerConnection> client) {
	bool nodeExists = configStore.existsNode(node);

	// Only allow operations on existing nodes, this is for remote
	// control, so we only manipulate what's already there!
	if (!nodeExists) {
		// Send back error message to client.
		caerConfigSendError(client, "Node doesn't exist. Operations are only allowed on existing data.");
	}

	return (nodeExists);
}

static inline bool checkAttributeExists(
	dvCfg::Node wantedNode, const std::string &key, dvCfgType type, std::shared_ptr<ConfigServerConnection> client) {
	// Check if attribute exists. Only allow operations on existing attributes!
	bool attrExists = wantedNode.existsAttribute(key, type);

	if (!attrExists) {
		// Send back error message to client.
		caerConfigSendError(
			client, "Attribute of given type doesn't exist. Operations are only allowed on existing data.");
	}

	return (attrExists);
}

static inline void caerConfigSendBoolResponse(
	std::shared_ptr<ConfigServerConnection> client, caerConfigAction action, bool result) {
	// Send back result to client. Format is the same as incoming data.
	caerConfigSendResponse(client, action, dvCfgType::BOOL, ((result) ? ("true") : ("false")));
}

void caerConfigServerHandleRequest(std::shared_ptr<ConfigServerConnection> client) {
	caerConfigActionData &data = client->getData();

	caerConfigAction action = data.getAction();
	dvCfgType type          = static_cast<dvCfgType>(data.getType());

	logger::log(
		logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Handling request from client: %s.", data.toString().c_str());

	// Interpretation of data is up to each action individually.
	dvCfg::Tree configStore = dvCfg::GLOBAL;

	switch (action) {
		case caerConfigAction::NODE_EXISTS: {
			// We only need the node name here. Type is not used (ignored)!
			bool result = configStore.existsNode(data.getNode());

			// Send back result to client. Format is the same as incoming data.
			caerConfigSendBoolResponse(client, caerConfigAction::NODE_EXISTS, result);

			break;
		}

		case caerConfigAction::ATTR_EXISTS: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(data.getNode());

			// Check if attribute exists.
			bool result = sshsNodeAttributeExists(wantedNode, data.getKey(), type);

			// Send back result to client. Format is the same as incoming data.
			caerConfigSendBoolResponse(client, caerConfigAction::ATTR_EXISTS, result);

			break;
		}

		case caerConfigAction::GET: {
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
				caerConfigSendResponse(client, caerConfigAction::GET, type, resultStr);

				free(resultStr);
			}

			// If this is a string, we must remember to free the original result.str
			// too, since it will also be a copy of the string coming from SSHS.
			if (type == SSHS_STRING) {
				free(result.string);
			}

			break;
		}

		case caerConfigAction::PUT: {
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
			caerConfigSendBoolResponse(client, caerConfigAction::PUT, true);

			break;
		}

		case caerConfigAction::GET_CHILDREN: {
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

			caerConfigSendResponse(
				client, caerConfigAction::GET_CHILDREN, SSHS_STRING, std::string(namesBuffer, namesLength - 1));

			break;
		}

		case caerConfigAction::GET_ATTRIBUTES: {
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

			caerConfigSendResponse(
				client, caerConfigAction::GET_ATTRIBUTES, SSHS_STRING, std::string(keysBuffer, keysLength - 1));

			break;
		}

		case caerConfigAction::GET_TYPE: {
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

			caerConfigSendResponse(client, caerConfigAction::GET_TYPE, SSHS_STRING, typeStr);

			break;
		}

		case caerConfigAction::GET_RANGES: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			struct sshs_node_attr_ranges ranges = sshsNodeGetAttributeRanges(wantedNode, data.getKey().c_str(), type);

			char *rangesString = sshsHelperRangesToStringConverter(type, ranges);

			std::string rangesStr(rangesString);

			free(rangesString);

			caerConfigSendResponse(client, caerConfigAction::GET_RANGES, type, rangesStr);

			break;
		}

		case caerConfigAction::GET_FLAGS: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			int flags = sshsNodeGetAttributeFlags(wantedNode, data.getKey().c_str(), type);

			char *flagsString = sshsHelperFlagsToStringConverter(flags);

			std::string flagsStr(flagsString);

			free(flagsString);

			caerConfigSendResponse(client, caerConfigAction::GET_FLAGS, SSHS_STRING, flagsStr);

			break;
		}

		case caerConfigAction::GET_DESCRIPTION: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			sshsNode wantedNode = sshsGetNode(configStore, data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			char *description = sshsNodeGetAttributeDescription(wantedNode, data.getKey().c_str(), type);

			caerConfigSendResponse(client, caerConfigAction::GET_DESCRIPTION, SSHS_STRING, description);

			free(description);

			break;
		}

		case caerConfigAction::ADD_MODULE: {
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
			caerConfigSendBoolResponse(client, caerConfigAction::ADD_MODULE, true);

			break;
		}

		case caerConfigAction::REMOVE_MODULE: {
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
			caerConfigSendBoolResponse(client, caerConfigAction::REMOVE_MODULE, true);

			break;
		}

		case caerConfigAction::ADD_PUSH_CLIENT: {
			client->addPushClient();

			// Send back confirmation to the client.
			caerConfigSendBoolResponse(client, caerConfigAction::ADD_PUSH_CLIENT, true);

			break;
		}

		case caerConfigAction::REMOVE_PUSH_CLIENT: {
			client->removePushClient();

			// Send back confirmation to the client.
			caerConfigSendBoolResponse(client, caerConfigAction::REMOVE_PUSH_CLIENT, true);

			break;
		}

		case caerConfigAction::DUMP_TREE: {
			// Run through the whole SSHS tree as it is currently and dump its content.

			break;
		}

		default: {
			// Unknown action, send error back to client.
			caerConfigSendError(client, "Unknown action.");

			break;
		}
	}
}
