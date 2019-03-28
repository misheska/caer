#include "config_server_actions.h"

#include "dv-sdk/cross/portable_io.h"

#include "../main.hpp"
#include "config_server_connection.h"
#include "config_server_main.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/tokenizer.hpp>
#include <regex>
#include <thread>

namespace logger = libcaer::log;
namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

static void dumpNodeToClientRecursive(const dvCfg::Node node, ConfigServerConnection *client);

template<typename MsgOps>
static inline void sendMessage(std::shared_ptr<ConfigServerConnection> client, MsgOps msgFunc) {
	// Send back flags directly.
	auto msgBuild = std::make_shared<flatbuffers::FlatBufferBuilder>(DV_CONFIG_SERVER_MAX_INCOMING_SIZE);

	// Build and then finish off message.
	auto msgRoot = msgFunc(msgBuild.get());

	// Write root node and message size.
	dv::FinishSizePrefixedConfigActionDataBuffer(*msgBuild, msgRoot);

	client->writeMessage(msgBuild);
}

static inline void sendError(
	const std::string &errorMsg, std::shared_ptr<ConfigServerConnection> client, uint64_t receivedID) {
	sendMessage(client, [errorMsg, receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
		auto valStr = msgBuild->CreateString(errorMsg);

		dv::ConfigActionDataBuilder msg(*msgBuild);

		msg.add_action(dv::ConfigAction::ERROR);
		msg.add_id(receivedID);
		msg.add_value(valStr);

		return (msg.Finish());
	});

	logger::log(logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Sent error back to client %lld: %s.",
		client->getClientID(), errorMsg.c_str());
}

static inline bool checkNodeExists(dvCfg::Tree configStore, const std::string &node,
	std::shared_ptr<ConfigServerConnection> client, uint64_t receivedID) {
	bool nodeExists = configStore.existsNode(node);

	// Only allow operations on existing nodes, this is for remote
	// control, so we only manipulate what's already there!
	if (!nodeExists) {
		// Send back error message to client.
		sendError("Node doesn't exist. Operations are only allowed on existing data.", client, receivedID);
	}

	return (nodeExists);
}

static inline bool checkAttributeExists(dvCfg::Node wantedNode, const std::string &key, dv::ConfigType type,
	std::shared_ptr<ConfigServerConnection> client, uint64_t receivedID) {
	// Check if attribute exists. Only allow operations on existing attributes!
	bool attrExists = wantedNode.existsAttribute(key, static_cast<dvCfgType>(type));

	if (!attrExists) {
		// Send back error message to client.
		sendError(
			"Attribute of given type doesn't exist. Operations are only allowed on existing data.", client, receivedID);
	}

	return (attrExists);
}

static inline const std::string getString(const flatbuffers::String *str,
	std::shared_ptr<ConfigServerConnection> client, uint64_t receivedID, bool allowEmptyString = false) {
	// Check if member is not defined/missing.
	if (str == nullptr) {
		sendError("Required string member missing.", client, receivedID);
		throw std::invalid_argument("Required string member missing.");
	}

	std::string s(str->string_view());

	if (!allowEmptyString && s.empty()) {
		sendError("String member empty.", client, receivedID);
		throw std::invalid_argument("String member empty.");
	}

	return (s);
}

void dvConfigServerHandleRequest(
	std::shared_ptr<ConfigServerConnection> client, std::unique_ptr<uint8_t[]> messageBuffer) {
	auto message = dv::GetConfigActionData(messageBuffer.get());

	dv::ConfigAction action = message->action();
	uint64_t receivedID     = message->id(); // Get incoming ID to send back.

	// TODO: better debug message.
	logger::log(
		logger::logLevel::DEBUG, CONFIG_SERVER_NAME, "Handling request from client %lld.", client->getClientID());

	// Interpretation of data is up to each action individually.
	dvCfg::Tree configStore = dvCfg::GLOBAL;

	switch (action) {
		case dv::ConfigAction::NODE_EXISTS: {
			std::string node;

			try {
				node = getString(message->node(), client, receivedID);
			}
			catch (const std::invalid_argument &) {
				break;
			}

			// We only need the node name here. Type is not used (ignored)!
			bool result = configStore.existsNode(node);

			// Send back result to client.
			sendMessage(client, [result, receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
				auto valStr = msgBuild->CreateString((result) ? ("true") : ("false"));

				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::NODE_EXISTS);
				msg.add_id(receivedID);
				msg.add_value(valStr);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::ATTR_EXISTS: {
			std::string node;
			std::string key;

			try {
				node = getString(message->node(), client, receivedID);
				key  = getString(message->key(), client, receivedID);
			}
			catch (const std::invalid_argument &) {
				break;
			}

			dv::ConfigType type = message->type();
			if (type == dv::ConfigType::UNKNOWN) {
				// Send back error message to client.
				sendError("Invalid type.", client, receivedID);
				break;
			}

			if (!checkNodeExists(configStore, node, client, receivedID)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			// Check if attribute exists.
			bool result = wantedNode.existsAttribute(key, static_cast<dvCfgType>(type));

			// Send back result to client.
			sendMessage(client, [result, receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
				auto valStr = msgBuild->CreateString((result) ? ("true") : ("false"));

				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::ATTR_EXISTS);
				msg.add_id(receivedID);
				msg.add_value(valStr);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::GET_CHILDREN: {
			std::string node;

			try {
				node = getString(message->node(), client, receivedID);
			}
			catch (const std::invalid_argument &) {
				break;
			}

			if (!checkNodeExists(configStore, node, client, receivedID)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			// Get the names of all the child nodes and return them.
			auto childNames = wantedNode.getChildNames();

			// No children at all, return empty.
			if (childNames.empty()) {
				// Send back error message to client.
				sendError("Node has no children.", client, receivedID);
				break;
			}

			// We need to return a big string with all of the child names,
			// separated by a | character.
			const std::string namesString = boost::algorithm::join(childNames, "|");

			sendMessage(client, [namesString, receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
				auto valStr = msgBuild->CreateString(namesString);

				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::GET_CHILDREN);
				msg.add_id(receivedID);
				msg.add_value(valStr);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::GET_ATTRIBUTES: {
			std::string node;

			try {
				node = getString(message->node(), client, receivedID);
			}
			catch (const std::invalid_argument &) {
				break;
			}

			if (!checkNodeExists(configStore, node, client, receivedID)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			// Get the keys of all the attributes and return them.
			auto attrKeys = wantedNode.getAttributeKeys();

			// No attributes at all, return empty.
			if (attrKeys.empty()) {
				// Send back error message to client.
				sendError("Node has no attributes.", client, receivedID);
				break;
			}

			// We need to return a big string with all of the attribute keys,
			// separated by a | character.
			const std::string attrKeysString = boost::algorithm::join(attrKeys, "|");

			sendMessage(client, [attrKeysString, receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
				auto valStr = msgBuild->CreateString(attrKeysString);

				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::GET_ATTRIBUTES);
				msg.add_id(receivedID);
				msg.add_value(valStr);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::GET_TYPE: {
			std::string node;
			std::string key;

			try {
				node = getString(message->node(), client, receivedID);
				key  = getString(message->key(), client, receivedID);
			}
			catch (const std::invalid_argument &) {
				break;
			}

			if (!checkNodeExists(configStore, node, client, receivedID)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			// Check if any keys match the given one and return its type.
			auto attrType = wantedNode.getAttributeType(key);

			// No attributes for specified key, return empty.
			if (attrType == dvCfgType::UNKNOWN) {
				// Send back error message to client.
				sendError("Node has no attribute with specified key.", client, receivedID);
				break;
			}

			// Send back type directly.
			sendMessage(client, [attrType, receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::GET_TYPE);
				msg.add_id(receivedID);
				msg.add_type(static_cast<dv::ConfigType>(attrType));

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::GET_RANGES: {
			std::string node;
			std::string key;

			try {
				node = getString(message->node(), client, receivedID);
				key  = getString(message->key(), client, receivedID);
			}
			catch (const std::invalid_argument &) {
				break;
			}

			dv::ConfigType type = message->type();
			if (type == dv::ConfigType::UNKNOWN) {
				// Send back error message to client.
				sendError("Invalid type.", client, receivedID);
				break;
			}

			if (!checkNodeExists(configStore, node, client, receivedID)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			if (!checkAttributeExists(wantedNode, key, type, client, receivedID)) {
				break;
			}

			struct dvConfigAttributeRanges ranges = wantedNode.getAttributeRanges(key, static_cast<dvCfgType>(type));

			const std::string rangesStr = dvCfg::Helper::rangesToStringConverter(static_cast<dvCfgType>(type), ranges);

			// Send back ranges as strings.
			sendMessage(client, [rangesStr, receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
				auto valStr = msgBuild->CreateString(rangesStr);

				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::GET_RANGES);
				msg.add_id(receivedID);
				msg.add_ranges(valStr);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::GET_FLAGS: {
			std::string node;
			std::string key;

			try {
				node = getString(message->node(), client, receivedID);
				key  = getString(message->key(), client, receivedID);
			}
			catch (const std::invalid_argument &) {
				break;
			}

			dv::ConfigType type = message->type();
			if (type == dv::ConfigType::UNKNOWN) {
				// Send back error message to client.
				sendError("Invalid type.", client, receivedID);
				break;
			}

			if (!checkNodeExists(configStore, node, client, receivedID)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			if (!checkAttributeExists(wantedNode, key, type, client, receivedID)) {
				break;
			}

			int flags = wantedNode.getAttributeFlags(key, static_cast<dvCfgType>(type));

			// Send back flags directly.
			sendMessage(client, [flags, receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::GET_FLAGS);
				msg.add_id(receivedID);
				msg.add_flags(flags);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::GET_DESCRIPTION: {
			std::string node;
			std::string key;

			try {
				node = getString(message->node(), client, receivedID);
				key  = getString(message->key(), client, receivedID);
			}
			catch (const std::invalid_argument &) {
				break;
			}

			dv::ConfigType type = message->type();
			if (type == dv::ConfigType::UNKNOWN) {
				// Send back error message to client.
				sendError("Invalid type.", client, receivedID);
				break;
			}

			if (!checkNodeExists(configStore, node, client, receivedID)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			if (!checkAttributeExists(wantedNode, key, type, client, receivedID)) {
				break;
			}

			const std::string description = wantedNode.getAttributeDescription(key, static_cast<dvCfgType>(type));

			// Send back flags directly.
			sendMessage(client, [description, receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
				auto valStr = msgBuild->CreateString(description);

				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::GET_DESCRIPTION);
				msg.add_id(receivedID);
				msg.add_description(valStr);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::GET: {
			std::string node;
			std::string key;

			try {
				node = getString(message->node(), client, receivedID);
				key  = getString(message->key(), client, receivedID);
			}
			catch (const std::invalid_argument &) {
				break;
			}

			dv::ConfigType type = message->type();
			if (type == dv::ConfigType::UNKNOWN) {
				// Send back error message to client.
				sendError("Invalid type.", client, receivedID);
				break;
			}

			if (!checkNodeExists(configStore, node, client, receivedID)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			if (!checkAttributeExists(wantedNode, key, type, client, receivedID)) {
				break;
			}

			union dvConfigAttributeValue result = wantedNode.getAttribute(key, static_cast<dvCfgType>(type));

			const std::string resultStr = dvCfg::Helper::valueToStringConverter(static_cast<dvCfgType>(type), result);

			if (type == dv::ConfigType::STRING) {
				free(result.string);
			}

			sendMessage(client, [resultStr, receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
				auto valStr = msgBuild->CreateString(resultStr);

				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::GET);
				msg.add_id(receivedID);
				msg.add_value(valStr);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::PUT: {
			// Check type first, needed for value check.
			dv::ConfigType type = message->type();
			if (type == dv::ConfigType::UNKNOWN) {
				// Send back error message to client.
				sendError("Invalid type.", client, receivedID);
				break;
			}

			std::string node;
			std::string key;
			std::string value;

			try {
				node  = getString(message->node(), client, receivedID);
				key   = getString(message->key(), client, receivedID);
				value = getString(
					message->value(), client, receivedID, (type == dv::ConfigType::STRING) ? (true) : (false));
			}
			catch (const std::invalid_argument &) {
				break;
			}

			// Support creating new nodes.
			bool import = (message->flags() & DVCFG_FLAGS_IMPORTED);

			if (!import && !checkNodeExists(configStore, node, client, receivedID)) {
				break;
			}

			// This cannot fail, since we know the node exists from above.
			dvCfg::Node wantedNode = configStore.getNode(node);

			if (!import && !checkAttributeExists(wantedNode, key, type, client, receivedID)) {
				break;
			}

			// Put given value into config node. Node, attr and type are already verified.
			const std::string typeStr = dvCfg::Helper::typeToStringConverter(static_cast<dvCfgType>(type));

			if (!wantedNode.stringToAttributeConverter(key, typeStr, value)) {
				// Send back correct error message to client.
				if (errno == EINVAL) {
					sendError("Impossible to convert value according to type.", client, receivedID);
					break;
				}
				else if (errno == EPERM) {
					// Suppress error message on initial import.
					// It is supposed to not overwrite READ_ONLY attributes ever.
					if (!import) {
						sendError("Cannot write to a read-only attribute.", client, receivedID);
						break;
					}
				}
				else if (errno == ERANGE) {
					sendError("Value out of attribute range.", client, receivedID);
					break;
				}
				else {
					// Unknown error.
					sendError("Unknown error.", client, receivedID);
					break;
				}
			}

			// Send back confirmation to the client.
			sendMessage(client, [receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::PUT);
				msg.add_id(receivedID);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::ADD_MODULE: {
			std::string moduleName;
			std::string moduleLibrary;

			try {
				moduleName    = getString(message->node(), client, receivedID);
				moduleLibrary = getString(message->key(), client, receivedID);
			}
			catch (const std::invalid_argument &) {
				break;
			}

			const std::regex moduleNameRegex("^[a-zA-Z-_\\d\\.]+$");

			if (!std::regex_match(moduleName, moduleNameRegex)) {
				sendError("Name uses invalid characters.", client, receivedID);
				break;
			}

			if (configStore.existsNode("/mainloop/" + moduleName + "/")) {
				sendError("Name is already in use.", client, receivedID);
				break;
			}

			// Check module library.
			auto modulesSysNode = configStore.getNode("/system/modules/");
			auto modulesList    = modulesSysNode.getChildNames();

			if (!dv::findBool(modulesList.begin(), modulesList.end(), moduleLibrary)) {
				sendError("Library does not exist.", client, receivedID);
				break;
			}

			// Name and library are fine, create the module.
			dv::addModule(moduleName, moduleLibrary);

			// Send back confirmation to the client.
			sendMessage(client, [receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::ADD_MODULE);
				msg.add_id(receivedID);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::REMOVE_MODULE: {
			std::string moduleName;

			try {
				moduleName = getString(message->node(), client, receivedID);
			}
			catch (const std::invalid_argument &) {
				break;
			}

			if (!configStore.existsNode("/mainloop/" + moduleName + "/")) {
				sendError("Name is not in use.", client, receivedID);
				break;
			}

			auto moduleNode = configStore.getNode("/mainloop/" + moduleName + "/");

			// Modules can only be deleted if not running.
			moduleNode.put<dvCfgType::BOOL>("running", false);

			// Wait for termination...
			while (moduleNode.get<dvCfgType::BOOL>("isRunning")) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			// Truly delete the node and all its children.
			dv::removeModule(moduleName);

			// Send back confirmation to the client.
			sendMessage(client, [receivedID](flatbuffers::FlatBufferBuilder *msgBuild) {
				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::REMOVE_MODULE);
				msg.add_id(receivedID);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::ADD_PUSH_CLIENT: {
			// Send back confirmation to the client.
			sendMessage(client, [](flatbuffers::FlatBufferBuilder *msgBuild) {
				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::ADD_PUSH_CLIENT);

				return (msg.Finish());
			});

			// Only add client after sending confirmation, so no PUSH
			// messages may arrive before the client sees the confirmation.
			client->addPushClient();

			break;
		}

		case dv::ConfigAction::REMOVE_PUSH_CLIENT: {
			// Remove client first, so that after confirmation of removal
			// no more PUSH messages may arrive.
			client->removePushClient();

			// Send back confirmation to the client.
			sendMessage(client, [](flatbuffers::FlatBufferBuilder *msgBuild) {
				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::REMOVE_PUSH_CLIENT);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::DUMP_TREE: {
			// Run through the whole ConfigTree as it is currently and dump its content.
			dumpNodeToClientRecursive(configStore.getRootNode(), client.get());

			// Send back confirmation of operation completed to the client.
			sendMessage(client, [](flatbuffers::FlatBufferBuilder *msgBuild) {
				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::DUMP_TREE);

				return (msg.Finish());
			});

			break;
		}

		case dv::ConfigAction::GET_CLIENT_ID: {
			uint64_t clientID = client->getClientID();

			// Send back confirmation of operation completed to the client.
			sendMessage(client, [clientID](flatbuffers::FlatBufferBuilder *msgBuild) {
				dv::ConfigActionDataBuilder msg(*msgBuild);

				msg.add_action(dv::ConfigAction::GET_CLIENT_ID);
				msg.add_id(clientID);

				return (msg.Finish());
			});

			break;
		}

		default: {
			// Unknown action, send error back to client.
			sendError("Unknown action.", client, receivedID);

			break;
		}
	}
}

static void dumpNodeToClientRecursive(const dvCfg::Node node, ConfigServerConnection *client) {
	// Dump node path.
	{
		auto msgBuild = std::make_shared<flatbuffers::FlatBufferBuilder>(DV_CONFIG_SERVER_MAX_INCOMING_SIZE);

		auto nodeStr = msgBuild->CreateString(node.getPath());

		dv::ConfigActionDataBuilder msg(*msgBuild);

		msg.add_action(dv::ConfigAction::DUMP_TREE_NODE);
		msg.add_node(nodeStr);

		// Finish off message.
		auto msgRoot = msg.Finish();

		// Write root node and message size.
		dv::FinishSizePrefixedConfigActionDataBuffer(*msgBuild, msgRoot);

		client->writePushMessage(msgBuild);
	}

	// Dump all attribute keys.
	for (const auto &key : node.getAttributeKeys()) {
		auto msgBuild = std::make_shared<flatbuffers::FlatBufferBuilder>(DV_CONFIG_SERVER_MAX_INCOMING_SIZE);

		auto type  = node.getAttributeType(key);
		auto flags = node.getAttributeFlags(key, type);

		union dvConfigAttributeValue value = node.getAttribute(key, type);
		const std::string valueStr         = dvCfg::Helper::valueToStringConverter(type, value);
		if (type == dvCfgType::STRING) {
			free(value.string);
		}

		auto nodeStr = msgBuild->CreateString(node.getPath());
		auto keyStr  = msgBuild->CreateString(key);
		auto valStr  = msgBuild->CreateString(valueStr);

		const std::string rangesStr = dvCfg::Helper::rangesToStringConverter(type, node.getAttributeRanges(key, type));
		auto ranStr                 = msgBuild->CreateString(rangesStr);

		const std::string descriptionStr = node.getAttributeDescription(key, type);
		auto descStr                     = msgBuild->CreateString(descriptionStr);

		dv::ConfigActionDataBuilder msg(*msgBuild);

		msg.add_action(dv::ConfigAction::DUMP_TREE_ATTR);
		msg.add_node(nodeStr);
		msg.add_key(keyStr);
		msg.add_type(static_cast<dv::ConfigType>(type));
		msg.add_value(valStr);

		// Need to get extra info when adding: flags, range, description.
		msg.add_flags(flags);
		msg.add_ranges(ranStr);
		msg.add_description(descStr);

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
