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

template<typename MsgOps>
static inline void sendMessage(std::shared_ptr<ConfigServerConnection> client, MsgOps msgFunc) {
	// Send back flags directly.
	auto msgBuild = std::make_shared<flatbuffers::FlatBufferBuilder>(CAER_CONFIG_SERVER_MAX_INCOMING_SIZE);

	dv::ConfigActionDataBuilder msg(*msgBuild);

	msgFunc(msg);

	// Finish off message.
	auto msgRoot = msg.Finish();

	// Write root node and message size.
	dv::FinishSizePrefixedConfigActionDataBuffer(*msgBuild, msgRoot);

	client->writeMessage(msgBuild);
}

static inline void caerConfigSendError(std::shared_ptr<ConfigServerConnection> client, const std::string &errorMsg) {
	sendMessage(client, [errorMsg](dv::ConfigActionDataBuilder &msg) {
		msg.add_action(dv::ConfigAction::ERROR);
		msg.add_value(msg.fbb_.CreateString(errorMsg));
	});

	logger::log(logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Sent error back to client %lld: %s.",
		client->getClientID(), errorMsg.c_str());
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

static inline bool checkAttributeExists(dvCfg::Node wantedNode, const std::string &key, dv::ConfigType type,
	std::shared_ptr<ConfigServerConnection> client) {
	// Check if attribute exists. Only allow operations on existing attributes!
	bool attrExists = wantedNode.existsAttribute(key, static_cast<dvCfgType>(type));

	if (!attrExists) {
		// Send back error message to client.
		caerConfigSendError(
			client, "Attribute of given type doesn't exist. Operations are only allowed on existing data.");
	}

	return (attrExists);
}

static inline const std::string getString(
	const flatbuffers::String *str, std::shared_ptr<ConfigServerConnection> client) {
	if (str == nullptr) {
		caerConfigSendError(client, "Required string member missing.");
		return (std::string());
	}

	return (str->str());
}

void caerConfigServerHandleRequest(
	std::shared_ptr<ConfigServerConnection> client, std::unique_ptr<uint8_t[]> messageBuffer) {
	auto message = dv::GetConfigActionData(messageBuffer.get());

	dv::ConfigAction action = message->action();

	// TODO: better debug message.
	logger::log(
		logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Handling request from client %lld.", client->getClientID());

	// Interpretation of data is up to each action individually.
	dvCfg::Tree configStore = dvCfg::GLOBAL;

	switch (action) {
		case dv::ConfigAction::NODE_EXISTS: {
			const std::string node = getString(message->node(), client);
			if (node.empty()) {
				break;
			}

			// We only need the node name here. Type is not used (ignored)!
			bool result = configStore.existsNode(node);

			// Send back result to client.
			sendMessage(client, [result](dv::ConfigActionDataBuilder &msg) {
				msg.add_action(dv::ConfigAction::NODE_EXISTS);
				msg.add_value(msg.fbb_.CreateString((result) ? ("true") : ("false")));
			});

			break;
		}

		case dv::ConfigAction::ATTR_EXISTS: {
			const std::string node = getString(message->node(), client);
			const std::string key  = getString(message->key(), client);
			if (node.empty() || key.empty()) {
				break;
			}

			dv::ConfigType type = message->type();
			if (type == dv::ConfigType::UNKNOWN) {
				// Send back error message to client.
				caerConfigSendError(client, "Invalid type.");
				break;
			}

			if (!checkNodeExists(configStore, node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			// Check if attribute exists.
			bool result = wantedNode.existsAttribute(key, static_cast<dvCfgType>(type));

			// Send back result to client.
			sendMessage(client, [result](dv::ConfigActionDataBuilder &msg) {
				msg.add_action(dv::ConfigAction::ATTR_EXISTS);
				msg.add_value(msg.fbb_.CreateString((result) ? ("true") : ("false")));
			});

			break;
		}

		case dv::ConfigAction::GET_CHILDREN: {
			const std::string node = getString(message->node(), client);
			if (node.empty()) {
				break;
			}

			if (!checkNodeExists(configStore, node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

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

			sendMessage(client, [namesString](dv::ConfigActionDataBuilder &msg) {
				msg.add_action(dv::ConfigAction::GET_CHILDREN);
				msg.add_value(msg.fbb_.CreateString(namesString));
			});

			break;
		}

		case dv::ConfigAction::GET_ATTRIBUTES: {
			const std::string node = getString(message->node(), client);
			if (node.empty()) {
				break;
			}

			if (!checkNodeExists(configStore, node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

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

			sendMessage(client, [attrKeysString](dv::ConfigActionDataBuilder &msg) {
				msg.add_action(dv::ConfigAction::GET_ATTRIBUTES);
				msg.add_value(msg.fbb_.CreateString(attrKeysString));
			});

			break;
		}

		case dv::ConfigAction::GET_TYPE: {
			const std::string node = getString(message->node(), client);
			const std::string key  = getString(message->key(), client);
			if (node.empty() || key.empty()) {
				break;
			}

			if (!checkNodeExists(configStore, node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			// Check if any keys match the given one and return its type.
			auto attrType = wantedNode.getAttributeType(key);

			// No attributes for specified key, return empty.
			if (attrType == dvCfgType::UNKNOWN) {
				// Send back error message to client.
				caerConfigSendError(client, "Node has no attribute with specified key.");
				break;
			}

			// Send back type directly.
			sendMessage(client, [attrType](dv::ConfigActionDataBuilder &msg) {
				msg.add_action(dv::ConfigAction::GET_TYPE);
				msg.add_type(static_cast<dv::ConfigType>(attrType));
			});

			break;
		}

		case dv::ConfigAction::GET_RANGES: {
			const std::string node = getString(message->node(), client);
			const std::string key  = getString(message->key(), client);
			if (node.empty() || key.empty()) {
				break;
			}

			dv::ConfigType type = message->type();
			if (type == dv::ConfigType::UNKNOWN) {
				// Send back error message to client.
				caerConfigSendError(client, "Invalid type.");
				break;
			}

			if (!checkNodeExists(configStore, node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			if (!checkAttributeExists(wantedNode, key, type, client)) {
				break;
			}

			struct dvConfigAttributeRanges ranges = wantedNode.getAttributeRanges(key, static_cast<dvCfgType>(type));

			const std::string rangesStr = dvCfg::Helper::rangesToStringConverter(static_cast<dvCfgType>(type), ranges);

			// Send back ranges as strings.
			sendMessage(client, [rangesStr](dv::ConfigActionDataBuilder &msg) {
				msg.add_action(dv::ConfigAction::GET_RANGES);
				msg.add_ranges(msg.fbb_.CreateString(rangesStr));
			});

			break;
		}

		case dv::ConfigAction::GET_FLAGS: {
			const std::string node = getString(message->node(), client);
			const std::string key  = getString(message->key(), client);
			if (node.empty() || key.empty()) {
				break;
			}

			dv::ConfigType type = message->type();
			if (type == dv::ConfigType::UNKNOWN) {
				// Send back error message to client.
				caerConfigSendError(client, "Invalid type.");
				break;
			}

			if (!checkNodeExists(configStore, node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			if (!checkAttributeExists(wantedNode, key, type, client)) {
				break;
			}

			int flags = wantedNode.getAttributeFlags(key, static_cast<dvCfgType>(type));

			// Send back flags directly.
			sendMessage(client, [flags](dv::ConfigActionDataBuilder &msg) {
				msg.add_action(dv::ConfigAction::GET_FLAGS);
				msg.add_flags(flags);
			});

			break;
		}

		case dv::ConfigAction::GET_DESCRIPTION: {
			const std::string node = getString(message->node(), client);
			const std::string key  = getString(message->key(), client);
			if (node.empty() || key.empty()) {
				break;
			}

			dv::ConfigType type = message->type();
			if (type == dv::ConfigType::UNKNOWN) {
				// Send back error message to client.
				caerConfigSendError(client, "Invalid type.");
				break;
			}

			if (!checkNodeExists(configStore, node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			if (!checkAttributeExists(wantedNode, key, type, client)) {
				break;
			}

			const std::string description = wantedNode.getAttributeDescription(key, static_cast<dvCfgType>(type));

			// Send back flags directly.
			sendMessage(client, [description](dv::ConfigActionDataBuilder &msg) {
				msg.add_action(dv::ConfigAction::GET_DESCRIPTION);
				msg.add_description(msg.fbb_.CreateString(description));
			});

			break;
		}

		case dv::ConfigAction::GET: {
			const std::string node = getString(message->node(), client);
			const std::string key  = getString(message->key(), client);
			if (node.empty() || key.empty()) {
				break;
			}

			dv::ConfigType type = message->type();
			if (type == dv::ConfigType::UNKNOWN) {
				// Send back error message to client.
				caerConfigSendError(client, "Invalid type.");
				break;
			}

			if (!checkNodeExists(configStore, node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			if (!checkAttributeExists(wantedNode, key, type, client)) {
				break;
			}

			union dvConfigAttributeValue result = wantedNode.getAttribute(key, static_cast<dvCfgType>(type));

			const std::string resultStr = dvCfg::Helper::valueToStringConverter(static_cast<dvCfgType>(type), result);

			sendMessage(client, [resultStr](dv::ConfigActionDataBuilder &msg) {
				msg.add_action(dv::ConfigAction::GET);
				msg.add_value(msg.fbb_.CreateString(resultStr));
			});

			break;
		}

		case dv::ConfigAction::PUT: {
			const std::string node  = getString(message->node(), client);
			const std::string key   = getString(message->key(), client);
			const std::string value = getString(message->value(), client);
			if (node.empty() || key.empty() || value.empty()) {
				break;
			}

			dv::ConfigType type = message->type();
			if (type == dv::ConfigType::UNKNOWN) {
				// Send back error message to client.
				caerConfigSendError(client, "Invalid type.");
				break;
			}

			if (!checkNodeExists(configStore, node, client)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			if (!checkAttributeExists(wantedNode, key, type, client)) {
				break;
			}

			// Put given value into config node. Node, attr and type are already verified.
			const std::string typeStr = dvCfg::Helper::typeToStringConverter(static_cast<dvCfgType>(type));

			if (!wantedNode.stringToAttributeConverter(key, typeStr, value)) {
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
			sendMessage(client, [](dv::ConfigActionDataBuilder &msg) { msg.add_action(dv::ConfigAction::PUT); });

			break;
		}

		case dv::ConfigAction::ADD_MODULE: {
			const std::string moduleName    = getString(message->node(), client);
			const std::string moduleLibrary = getString(message->key(), client);

			if (moduleName.empty()) {
				// Disallow empty strings.
				caerConfigSendError(client, "Name cannot be empty.");
				break;
			}

			if (moduleLibrary.empty()) {
				// Disallow empty strings.
				caerConfigSendError(client, "Library cannot be empty.");
				break;
			}

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
			sendMessage(client, [](dv::ConfigActionDataBuilder &msg) { msg.add_action(dv::ConfigAction::ADD_MODULE); });

			break;
		}

		case dv::ConfigAction::REMOVE_MODULE: {
			const std::string moduleName = getString(message->node(), client);

			if (moduleName.empty()) {
				// Disallow empty strings.
				caerConfigSendError(client, "Name cannot be empty.");
				break;
			}

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
			sendMessage(
				client, [](dv::ConfigActionDataBuilder &msg) { msg.add_action(dv::ConfigAction::REMOVE_MODULE); });

			break;
		}

		case dv::ConfigAction::ADD_PUSH_CLIENT: {
			client->addPushClient();

			// Send back confirmation to the client.
			sendMessage(
				client, [](dv::ConfigActionDataBuilder &msg) { msg.add_action(dv::ConfigAction::ADD_PUSH_CLIENT); });

			break;
		}

		case dv::ConfigAction::REMOVE_PUSH_CLIENT: {
			client->removePushClient();

			// Send back confirmation to the client.
			sendMessage(
				client, [](dv::ConfigActionDataBuilder &msg) { msg.add_action(dv::ConfigAction::REMOVE_PUSH_CLIENT); });

			break;
		}

		case dv::ConfigAction::DUMP_TREE: {
			// Run through the whole ConfigTree as it is currently and dump its content.
			dumpNodeToClientRecursive(configStore.getRootNode(), client.get());

			// Send back confirmation of operation completed to the client.
			sendMessage(client, [](dv::ConfigActionDataBuilder &msg) { msg.add_action(dv::ConfigAction::DUMP_TREE); });

			break;
		}

		case dv::ConfigAction::GET_CLIENT_ID: {
			uint64_t clientID = client->getClientID();

			// Send back confirmation of operation completed to the client.
			sendMessage(client, [clientID](dv::ConfigActionDataBuilder &msg) {
				msg.add_action(dv::ConfigAction::GET_CLIENT_ID);
				msg.add_id(clientID);
			});

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
		auto msgBuild = std::make_shared<flatbuffers::FlatBufferBuilder>(CAER_CONFIG_SERVER_MAX_INCOMING_SIZE);

		dv::ConfigActionDataBuilder msg(*msgBuild);

		msg.add_action(dv::ConfigAction::DUMP_TREE_NODE);

		msg.add_node(msgBuild->CreateString(node.getPath()));

		// Finish off message.
		auto msgRoot = msg.Finish();

		// Write root node and message size.
		dv::FinishSizePrefixedConfigActionDataBuffer(*msgBuild, msgRoot);

		client->writePushMessage(msgBuild);
	}

	// Dump all attribute keys.
	for (const auto &key : node.getAttributeKeys()) {
		auto msgBuild = std::make_shared<flatbuffers::FlatBufferBuilder>(CAER_CONFIG_SERVER_MAX_INCOMING_SIZE);

		dv::ConfigActionDataBuilder msg(*msgBuild);

		msg.add_action(dv::ConfigAction::DUMP_TREE_ATTR);

		msg.add_node(msgBuild->CreateString(node.getPath()));

		msg.add_key(msgBuild->CreateString(key));

		auto type = node.getAttributeType(key);

		msg.add_type(static_cast<dv::ConfigType>(type));

		const std::string valueStr = dvCfg::Helper::valueToStringConverter(type, node.getAttribute(key, type));

		msg.add_value(msgBuild->CreateString(valueStr));

		// Need to get extra info when adding: flags, range, description.
		const int flags = node.getAttributeFlags(key, type);

		msg.add_flags(flags);

		const std::string rangesStr = dvCfg::Helper::rangesToStringConverter(type, node.getAttributeRanges(key, type));

		msg.add_ranges(msgBuild->CreateString(rangesStr));

		const std::string descriptionStr = node.getAttributeDescription(key, type);

		msg.add_description(msgBuild->CreateString(descriptionStr));

		// Finish off message.
		auto msgRoot = msg.Finish();

		// Write root node and message size.
		dv::FinishSizePrefixedConfigActionDataBuffer(*msgBuild, msgRoot);

		client->writePushMessage(msgBuild);
	}

	// Recurse over all children.
	for (const auto &child : node.getChildren()) {
		dumpNodeToClientRecursive(child, client);
	}
}
