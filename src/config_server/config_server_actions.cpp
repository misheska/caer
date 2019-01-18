#include "config_server_actions.h"

#include "caer-sdk/cross/portable_io.h"

#include "../module.h"
#include "config_server_connection.h"
#include "config_server_main.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/tokenizer.hpp>
#include <regex>

namespace logger = libcaer::log;
namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

static void dumpNodeToClientRecursive(const dvCfg::Node node, ConfigServerConnection *client);

static inline void caerConfigSendError(std::shared_ptr<ConfigServerConnection> client, const std::string &errorMsg) {
	caerConfigActionData &response = client->getData();
	response.reset();

	response.setAction(caerConfigAction::ERROR);
	response.setType(dvCfgType::STRING);
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
	response.setType(type);
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
			bool result = wantedNode.existsAttribute(data.getKey(), type);

			// Send back result to client. Format is the same as incoming data.
			caerConfigSendBoolResponse(client, caerConfigAction::ATTR_EXISTS, result);

			break;
		}

		case caerConfigAction::GET: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			union dvConfigAttributeValue result = wantedNode.getAttribute(data.getKey(), type);

			const std::string resultStr = dvCfg::Helper::valueToStringConverter(type, result);

			caerConfigSendResponse(client, caerConfigAction::GET, type, resultStr);
			break;
		}

		case caerConfigAction::PUT: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			// Put given value into config node. Node, attr and type are already verified.
			const std::string typeStr = dvCfg::Helper::typeToStringConverter(type);

			if (!wantedNode.stringToAttributeConverter(data.getKey(), typeStr, data.getValue())) {
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
			dvCfg::Node wantedNode = configStore.getNode(data.getNode());

			// Get the names of all the child nodes and return them.
			auto childNames = wantedNode.getChildNames();

			// No children at all, return empty.
			if (childNames.empty()) {
				// Send back error message to client.
				caerConfigSendError(client, "Node has no children.");

				break;
			}

			// We need to return a big string with all of the child names,
			// separated by a | character.
			const std::string namesString = boost::algorithm::join(childNames, "|");

			caerConfigSendResponse(client, caerConfigAction::GET_CHILDREN, dvCfgType::STRING, namesString);

			break;
		}

		case caerConfigAction::GET_ATTRIBUTES: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(data.getNode());

			// Get the keys of all the attributes and return them.
			auto attrKeys = wantedNode.getAttributeKeys();

			// No attributes at all, return empty.
			if (attrKeys.empty()) {
				// Send back error message to client.
				caerConfigSendError(client, "Node has no attributes.");

				break;
			}

			// We need to return a big string with all of the attribute keys,
			// separated by a | character.
			const std::string attrKeysString = boost::algorithm::join(attrKeys, "|");

			caerConfigSendResponse(client, caerConfigAction::GET_ATTRIBUTES, dvCfgType::STRING, attrKeysString);

			break;
		}

		case caerConfigAction::GET_TYPE: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(data.getNode());

			// Check if any keys match the given one and return its type.
			auto attrType = wantedNode.getAttributeType(data.getKey());

			// No attributes for specified key, return empty.
			if (attrType == dvCfgType::UNKNOWN) {
				// Send back error message to client.
				caerConfigSendError(client, "Node has no attributes with specified key.");

				break;
			}

			// We need to return a string with the attribute type,
			// separated by a NUL character.
			const std::string typeStr = dvCfg::Helper::typeToStringConverter(attrType);

			caerConfigSendResponse(client, caerConfigAction::GET_TYPE, dvCfgType::STRING, typeStr);

			break;
		}

		case caerConfigAction::GET_RANGES: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			struct dvConfigAttributeRanges ranges = wantedNode.getAttributeRanges(data.getKey(), type);

			const std::string rangesStr = dvCfg::Helper::rangesToStringConverter(type, ranges);

			caerConfigSendResponse(client, caerConfigAction::GET_RANGES, type, rangesStr);

			break;
		}

		case caerConfigAction::GET_FLAGS: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			int flags = wantedNode.getAttributeFlags(data.getKey(), type);

			const std::string flagsStr = dvCfg::Helper::flagsToStringConverter(flags);

			caerConfigSendResponse(client, caerConfigAction::GET_FLAGS, dvCfgType::STRING, flagsStr);

			break;
		}

		case caerConfigAction::GET_DESCRIPTION: {
			if (!checkNodeExists(configStore, data.getNode(), client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(data.getNode());

			if (!checkAttributeExists(wantedNode, data.getKey(), type, client)) {
				break;
			}

			const std::string description = wantedNode.getAttributeDescription(data.getKey(), type);

			caerConfigSendResponse(client, caerConfigAction::GET_DESCRIPTION, dvCfgType::STRING, description);

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

			if (configStore.existsNode("/" + moduleName + "/")) {
				caerConfigSendError(client, "Name is already in use.");
				break;
			}

			// Check module library.
			auto modulesSysNode                  = configStore.getNode("/caer/modules/");
			const std::string modulesListOptions = modulesSysNode.get<dvCfgType::STRING>("modulesListOptions");

			boost::tokenizer<boost::char_separator<char>> modulesList(
				modulesListOptions, boost::char_separator<char>(","));

			if (!findBool(modulesList.begin(), modulesList.end(), moduleLibrary)) {
				caerConfigSendError(client, "Library does not exist.");
				break;
			}

			// Name and library are fine, let's determine the next free ID.
			auto rootNodes = configStore.getRootNode().getChildren();

			std::vector<int16_t> usedModuleIDs;

			for (const auto &mNode : rootNodes) {
				if (!mNode.exists<dvCfgType::INT>("moduleId")) {
					continue;
				}

				int16_t moduleID = I16T(mNode.get<dvCfgType::INT>("moduleId"));
				usedModuleIDs.push_back(moduleID);
			}

			vectorSortUnique(usedModuleIDs);

			size_t idx               = 0;
			int16_t nextFreeModuleID = 1;

			while (idx < usedModuleIDs.size() && usedModuleIDs[idx] == nextFreeModuleID) {
				idx++;
				nextFreeModuleID++;
			}

			// Let's create the needed configuration for the new module.
			auto newModuleNode = configStore.getNode("/" + moduleName + "/");

			newModuleNode.create<dvCfgType::INT>(
				"moduleId", nextFreeModuleID, {1, INT16_MAX}, dvCfgFlags::READ_ONLY, "Module ID.");
			newModuleNode.create<dvCfgType::STRING>(
				"moduleLibrary", moduleLibrary, {1, PATH_MAX}, dvCfgFlags::READ_ONLY, "Module library.");

			// Add moduleInput/moduleOutput as appropriate.
			auto moduleSysNode          = modulesSysNode.getRelativeNode(moduleLibrary + "/");
			const std::string inputType = moduleSysNode.get<dvCfgType::STRING>("type");

			if (inputType != "INPUT") {
				// CAER_MODULE_OUTPUT / CAER_MODULE_PROCESSOR
				// moduleInput must exist for OUTPUT and PROCESSOR modules.
				newModuleNode.create<dvCfgType::STRING>(
					"moduleInput", "", {0, 1024}, dvCfgFlags::NORMAL, "Module dynamic input definition.");
			}

			if (inputType != "OUTPUT") {
				// CAER_MODULE_INPUT / CAER_MODULE_PROCESSOR
				// moduleOutput must exist for INPUT and PROCESSOR modules, only
				// if their outputs are undefined (-1).
				if (moduleSysNode.existsRelativeNode("outputStreams/0/")) {
					auto outputNode0        = moduleSysNode.getRelativeNode("outputStreams/0/");
					int32_t outputNode0Type = outputNode0.get<dvCfgType::INT>("type");

					if (outputNode0Type == -1) {
						newModuleNode.create<dvCfgType::STRING>(
							"moduleOutput", "", {0, 1024}, dvCfgFlags::NORMAL, "Module dynamic output definition.");
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

			if (!configStore.existsNode("/" + moduleName + "/")) {
				caerConfigSendError(client, "Name is not in use.");
				break;
			}

			// Modules can only be deleted while the mainloop is not running, to not
			// destroy data the system is relying on.
			bool isMainloopRunning = configStore.getRootNode().get<dvCfgType::BOOL>("running");
			if (isMainloopRunning) {
				caerConfigSendError(client, "Mainloop is running.");
				break;
			}

			// Truly delete the node and all its children.
			configStore.getNode("/" + moduleName + "/").removeNode();

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
			// Run through the whole ConfigTree as it is currently and dump its content.
			dumpNodeToClientRecursive(configStore.getRootNode(), client.get());

			// Send back confirmation of operation completed to the client.
			caerConfigSendBoolResponse(client, caerConfigAction::DUMP_TREE, true);

			break;
		}

		default: {
			// Unknown action, send error back to client.
			caerConfigSendError(client, "Unknown action.");

			break;
		}
	}
}

static void dumpNodeToClientRecursive(const dvCfg::Node node, ConfigServerConnection *client) {
	// Dump node path.
	{
		auto msgNode = std::make_shared<caerConfigActionData>();

		msgNode->setAction(caerConfigAction::DUMP_TREE_NODE);
		msgNode->setNode(node.getPath());

		client->writePushMessage(msgNode);
	}

	// Dump all attribute keys.
	for (const auto &key : node.getAttributeKeys()) {
		auto type = node.getAttributeType(key);

		auto msgAttr = std::make_shared<caerConfigActionData>();

		msgAttr->setAction(caerConfigAction::DUMP_TREE_ATTR);
		msgAttr->setType(type);

		// Need to get extra info when adding: flags, range, description.
		const std::string flagsStr = dvCfg::Helper::flagsToStringConverter(node.getAttributeFlags(key, type));

		const std::string rangesStr = dvCfg::Helper::rangesToStringConverter(type, node.getAttributeRanges(key, type));

		const std::string descriptionStr = node.getAttributeDescription(key, type);

		std::string extraStr(flagsStr);
		extraStr.push_back('\0');
		extraStr += rangesStr;
		extraStr.push_back('\0');
		extraStr += descriptionStr;

		msgAttr->setExtra(extraStr);

		msgAttr->setNode(node.getPath());
		msgAttr->setKey(key);

		const std::string valueStr = dvCfg::Helper::valueToStringConverter(type, node.getAttribute(key, type));

		msgAttr->setValue(valueStr);

		client->writePushMessage(msgAttr);
	}

	// Recurse over all children.
	for (const auto &child : node.getChildren()) {
		dumpNodeToClientRecursive(child, client);
	}
}
