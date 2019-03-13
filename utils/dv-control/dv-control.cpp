#include "dv-sdk/cross/portable_io.h"
#include "dv-sdk/utils.h"

#include "../../src/config_server/dv_config_action_data.hpp"
#include "utils/ext/linenoise-ng/linenoise.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace asio    = boost::asio;
namespace asioSSL = boost::asio::ssl;
namespace asioIP  = boost::asio::ip;
using asioTCP     = boost::asio::ip::tcp;
namespace po      = boost::program_options;

#define DVCTL_HISTORY_FILE_NAME ".dv-control.history"
#define DVCTL_CLIENT_BUFFER_MAX_SIZE 8192

static void handleInputLine(const char *buf, size_t bufLength);
static void handleCommandCompletion(const char *buf, linenoiseCompletions *autoComplete);

static void actionCompletion(
	const std::string &buf, linenoiseCompletions *autoComplete, const std::string &partialActionString);
static void nodeCompletion(const std::string &buf, linenoiseCompletions *autoComplete, dv::ConfigAction action,
	const std::string &partialNodeString);
static void keyCompletion(const std::string &buf, linenoiseCompletions *autoComplete, dv::ConfigAction action,
	const std::string &nodeString, const std::string &partialKeyString);
static void typeCompletion(const std::string &buf, linenoiseCompletions *autoComplete, dv::ConfigAction action,
	const std::string &nodeString, const std::string &keyString, const std::string &partialTypeString);
static void valueCompletion(const std::string &buf, linenoiseCompletions *autoComplete, dv::ConfigAction action,
	const std::string &nodeString, const std::string &keyString, const std::string &typeString,
	const std::string &partialValueString);
static void addCompletionSuffix(linenoiseCompletions *autocomplete, const char *buf, size_t completionPoint,
	const char *suffix, bool endSpace, bool endSlash);

static const struct {
	const std::string name;
	const dv::ConfigAction code;
} actions[] = {
	{"node_exists", dv::ConfigAction::NODE_EXISTS},
	{"attr_exists", dv::ConfigAction::ATTR_EXISTS},
	{"get", dv::ConfigAction::GET},
	{"put", dv::ConfigAction::PUT},
	{"help", dv::ConfigAction::GET_DESCRIPTION},
	{"add_module", dv::ConfigAction::ADD_MODULE},
	{"remove_module", dv::ConfigAction::REMOVE_MODULE},
	{"get_client_id", dv::ConfigAction::GET_CLIENT_ID},
	{"dump_tree", dv::ConfigAction::DUMP_TREE},
};

static flatbuffers::FlatBufferBuilder dataBufferSend{DVCTL_CLIENT_BUFFER_MAX_SIZE};
static uint8_t dataBufferReceive[DVCTL_CLIENT_BUFFER_MAX_SIZE];

static asio::io_service ioService;
static asioSSL::context tlsContext{asioSSL::context::tlsv12_client};
static asioSSL::stream<asioTCP::socket> tlsSocket{ioService, tlsContext};
static bool secureConnection{false};

[[noreturn]] static inline void printHelpAndExit(const po::options_description &desc) {
	std::cout << std::endl << desc << std::endl;
	exit(EXIT_FAILURE);
}

static inline void asioSocketWrite(const asio::const_buffer &buf) {
	const asio::const_buffers_1 buf2(buf);

	if (secureConnection) {
		asio::write(tlsSocket, buf2);
	}
	else {
		asio::write(tlsSocket.next_layer(), buf2);
	}
}

static inline void asioSocketRead(const asio::mutable_buffer &buf) {
	const asio::mutable_buffers_1 buf2(buf);

	if (secureConnection) {
		asio::read(tlsSocket, buf2);
	}
	else {
		asio::read(tlsSocket.next_layer(), buf2);
	}
}

static inline void sendMessage(std::unique_ptr<dv::ConfigActionDataBuilder> msg) {
	// Finish off message.
	auto msgRoot = msg->Finish();

	// Write root node and message size.
	dv::FinishSizePrefixedConfigActionDataBuffer(dataBufferSend, msgRoot);

	// Send data out over the network.
	asioSocketWrite(asio::buffer(dataBufferSend.GetBufferPointer(), dataBufferSend.GetSize()));

	// Clear flatbuffer for next usage.
	dataBufferSend.Clear();
}

static inline const dv::ConfigActionData *receiveMessage() {
	flatbuffers::uoffset_t incomingMessageSize;

	// Get message size.
	asioSocketRead(asio::buffer(&incomingMessageSize, sizeof(incomingMessageSize)));

	// Check for wrong (excessive) message length.
	if (incomingMessageSize > DVCTL_CLIENT_BUFFER_MAX_SIZE) {
		throw std::runtime_error("Message length error (%d bytes).");
	}

	// Get actual data.
	asioSocketRead(asio::buffer(dataBufferReceive, incomingMessageSize));

	// Now we have the flatbuffer message and can verify it.
	flatbuffers::Verifier verifyMessage(dataBufferReceive, incomingMessageSize);

	if (!dv::VerifyConfigActionDataBuffer(verifyMessage)) {
		// Failed verification.
		throw std::runtime_error("Message verification error.");
	}

	auto message = dv::GetConfigActionData(dataBufferReceive);

	return (message);
}

int main(int argc, char *argv[]) {
	// Allowed command-line options for dv-control.
	po::options_description cliDescription("Command-line options");
	cliDescription.add_options()("help,h", "print help text")("ipaddress,i", po::value<std::string>(),
		"IP-address or hostname to connect to")("port,p", po::value<std::string>(), "port to connect to")("tls",
		po::value<std::string>()->implicit_value(""),
		"enable TLS for connection (no argument uses default CA for verification, or pass a path to a specific CA file "
		"in the PEM format)")(
		"tlscert", po::value<std::string>(), "TLS certificate file for client authentication (PEM format)")(
		"tlskey", po::value<std::string>(), "TLS key file for client authentication (PEM format)")("script,s",
		po::value<std::vector<std::string>>()->multitoken(),
		"script mode, sends the given command directly to the server as if typed in and exits.\n"
		"Format: <action> <node> [<attribute> <type> [<value>]]\nExample: set /system/logger/ logLevel byte 7");

	po::variables_map cliVarMap;
	try {
		po::store(boost::program_options::parse_command_line(argc, argv, cliDescription), cliVarMap);
		po::notify(cliVarMap);
	}
	catch (...) {
		std::cout << "Failed to parse command-line options!" << std::endl;
		printHelpAndExit(cliDescription);
	}

	// Parse/check command-line options.
	if (cliVarMap.count("help")) {
		printHelpAndExit(cliDescription);
	}

	std::string ipAddress("127.0.0.1");
	if (cliVarMap.count("ipaddress")) {
		ipAddress = cliVarMap["ipaddress"].as<std::string>();
	}

	std::string portNumber("4040");
	if (cliVarMap.count("port")) {
		portNumber = cliVarMap["port"].as<std::string>();
	}

	if (cliVarMap.count("tls")) {
		secureConnection = true;

		// Client-side TLS authentication support.
		if (cliVarMap.count("tlscert")) {
			std::string tlsCertFile = cliVarMap["tlscert"].as<std::string>();

			try {
				tlsContext.use_certificate_chain_file(tlsCertFile);
			}
			catch (const boost::system::system_error &ex) {
				boost::format exMsg
					= boost::format("Failed to load TLS client certificate file '%s', error message is:\n\t%s.")
					  % tlsCertFile % ex.what();
				std::cerr << exMsg.str() << std::endl;
				return (EXIT_FAILURE);
			}
		}

		if (cliVarMap.count("tlskey")) {
			std::string tlsKeyFile = cliVarMap["tlskey"].as<std::string>();

			try {
				tlsContext.use_private_key_file(tlsKeyFile, asioSSL::context::pem);
			}
			catch (const boost::system::system_error &ex) {
				boost::format exMsg = boost::format("Failed to load TLS client key file '%s', error message is:\n\t%s.")
									  % tlsKeyFile % ex.what();
				std::cerr << exMsg.str() << std::endl;
				return (EXIT_FAILURE);
			}
		}

		tlsContext.set_options(asioSSL::context::default_workarounds | asioSSL::context::single_dh_use);

		std::string tlsVerifyFile = cliVarMap["tls"].as<std::string>();

		if (tlsVerifyFile.empty()) {
			tlsContext.set_default_verify_paths();
		}
		else {
			try {
				tlsContext.load_verify_file(tlsVerifyFile);
			}
			catch (const boost::system::system_error &ex) {
				boost::format exMsg
					= boost::format("Failed to load TLS CA verification file '%s', error message is:\n\t%s.")
					  % tlsVerifyFile % ex.what();
				std::cerr << exMsg.str() << std::endl;
				return (EXIT_FAILURE);
			}
		}

		tlsContext.set_verify_mode(asioSSL::context::verify_peer);

		// Rebuild TLS socket, so it picks up changes to TLS context above.
		new (&tlsSocket) asioSSL::stream<asioTCP::socket>(ioService, tlsContext);
	}

	bool scriptMode = false;
	if (cliVarMap.count("script")) {
		std::vector<std::string> commandComponents = cliVarMap["script"].as<std::vector<std::string>>();

		// At lest two components must be passed, any less is an error.
		if (commandComponents.size() < 2) {
			std::cout << "Script mode must have at least two components!" << std::endl;
			printHelpAndExit(cliDescription);
		}

		// At most five components can be passed, any more is an error.
		if (commandComponents.size() > 5) {
			std::cout << "Script mode cannot have more than five components!" << std::endl;
			printHelpAndExit(cliDescription);
		}

		if (commandComponents[0] == "quit" || commandComponents[0] == "exit") {
			std::cout << "Script mode cannot use 'quit' or 'exit' actions!" << std::endl;
			printHelpAndExit(cliDescription);
		}

		scriptMode = true;
	}

	// Generate command history file path (in user home).
	boost::filesystem::path commandHistoryFilePath;

	try {
		char *userHome         = portable_get_user_home_directory();
		commandHistoryFilePath = userHome;
		free(userHome);
	}
	catch (const boost::filesystem::filesystem_error &) {
		std::cerr << "Failed to get home directory for history file, using current working directory." << std::endl;
		commandHistoryFilePath = boost::filesystem::current_path();
	}

	commandHistoryFilePath.append(DVCTL_HISTORY_FILE_NAME, boost::filesystem::path::codecvt());

	// Connect to the remote DV config server.
	try {
		asioTCP::resolver resolver(ioService);
		asio::connect(tlsSocket.next_layer(), resolver.resolve({ipAddress, portNumber}));
	}
	catch (const boost::system::system_error &ex) {
		boost::format exMsg = boost::format("Failed to connect to %s:%s, error message is:\n\t%s.") % ipAddress
							  % portNumber % ex.what();
		std::cerr << exMsg.str() << std::endl;
		return (EXIT_FAILURE);
	}

	// Secure connection support.
	if (secureConnection) {
		try {
			tlsSocket.handshake(asioSSL::stream_base::client);
		}
		catch (const boost::system::system_error &ex) {
			boost::format exMsg = boost::format("Failed TLS handshake, error message is:\n\t%s.") % ex.what();
			std::cerr << exMsg.str() << std::endl;
			return (EXIT_FAILURE);
		}
	}

	// Load command history file.
	linenoiseHistoryLoad(commandHistoryFilePath.string().c_str());

	if (scriptMode) {
		std::vector<std::string> commandComponents = cliVarMap["script"].as<std::vector<std::string>>();

		std::string inputString = boost::algorithm::join(commandComponents, " ");
		const char *inputLine   = inputString.c_str();

		// Add input to command history.
		linenoiseHistoryAdd(inputLine);

		// Try to generate a request, if there's any content.
		size_t inputLineLength = strlen(inputLine);

		if (inputLineLength > 0) {
			handleInputLine(inputLine, inputLineLength);
		}
	}
	else {
		// Create a shell prompt with the IP:Port displayed.
		boost::format shellPrompt = boost::format("DV @ %s:%s >> ") % ipAddress % portNumber;

		// Set our own command completion function.
		linenoiseSetCompletionCallback(&handleCommandCompletion);

		while (true) {
			// Display prompt and read input (NOTE: remember to free input after use!).
			char *inputLine = linenoise(shellPrompt.str().c_str());

			// Check for EOF first.
			if (inputLine == nullptr) {
				// Exit loop.
				break;
			}

			// Add input to command history.
			linenoiseHistoryAdd(inputLine);

			// Then, after having added to history, check for termination commands.
			if (strncmp(inputLine, "quit", 4) == 0 || strncmp(inputLine, "exit", 4) == 0) {
				// Exit loop, free memory.
				free(inputLine);
				break;
			}

			// Try to generate a request, if there's any content.
			size_t inputLineLength = strlen(inputLine);

			if (inputLineLength > 0) {
				handleInputLine(inputLine, inputLineLength);
			}

			// Free input after use.
			free(inputLine);
		}
	}

	// Save command history file.
	linenoiseHistorySave(commandHistoryFilePath.string().c_str());

	// Close secure connection properly.
	if (secureConnection) {
		boost::system::error_code error;
		tlsSocket.shutdown(error);

		// EOF is expected for good TLS shutdown. See:
		// https://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
		if (error != asio::error::eof) {
			boost::format errMsg = boost::format("Failed TLS shutdown, error message is:\n\t%s.") % error.message();
			std::cerr << errMsg.str() << std::endl;
		}
	}

	return (EXIT_SUCCESS);
}

#define MAX_CMD_PARTS 5

#define CMD_PART_ACTION 0
#define CMD_PART_NODE 1
#define CMD_PART_KEY 2
#define CMD_PART_TYPE 3
#define CMD_PART_VALUE 4

static void handleInputLine(const char *buf, size_t bufLength) {
	// First let's split up the command into its constituents.
	char *commandParts[MAX_CMD_PARTS + 1] = {nullptr};

	// Create a copy of buf, so that strtok_r() can modify it.
	char bufCopy[bufLength + 1];
	strcpy(bufCopy, buf);

	// Split string into usable parts.
	size_t idx         = 0;
	char *tokenSavePtr = nullptr, *nextCmdPart = nullptr, *currCmdPart = bufCopy;
	while ((nextCmdPart = strtok_r(currCmdPart, " ", &tokenSavePtr)) != nullptr) {
		if (idx < MAX_CMD_PARTS) {
			commandParts[idx] = nextCmdPart;
		}
		else {
			// Abort, too many parts.
			std::cerr << "Error: command is made up of too many parts." << std::endl;
			return;
		}

		idx++;
		currCmdPart = nullptr;
	}

	// Check that we got something.
	if (commandParts[CMD_PART_ACTION] == nullptr) {
		std::cerr << "Error: empty command." << std::endl;
		return;
	}

	// Let's get the action code first thing.
	dv::ConfigAction action = dv::ConfigAction::ERROR;

	for (const auto &act : actions) {
		if (act.name == commandParts[CMD_PART_ACTION]) {
			action = act.code;
		}
	}

	// Initialize buffer.
	std::unique_ptr<dv::ConfigActionDataBuilder> msg{nullptr};

	// Now that we know what we want to do, let's decode the command line.
	switch (action) {
		case dv::ConfigAction::GET_CLIENT_ID:
		case dv::ConfigAction::DUMP_TREE:
			// No parameters needed.
			if (commandParts[CMD_PART_NODE] != nullptr) {
				std::cerr << "Error: too many parameters for command." << std::endl;
				return;
			}

			msg = std::make_unique<dv::ConfigActionDataBuilder>(dataBufferSend);
			msg->add_action(action);

			break;

		case dv::ConfigAction::NODE_EXISTS: {
			// Check parameters needed for operation.
			if (commandParts[CMD_PART_NODE] == nullptr) {
				std::cerr << "Error: missing node parameter." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_NODE + 1] != nullptr) {
				std::cerr << "Error: too many parameters for command." << std::endl;
				return;
			}

			auto nodeOffset = dataBufferSend.CreateString(commandParts[CMD_PART_NODE]);

			msg = std::make_unique<dv::ConfigActionDataBuilder>(dataBufferSend);
			msg->add_action(action);
			msg->add_node(nodeOffset);

			break;
		}

		case dv::ConfigAction::ATTR_EXISTS:
		case dv::ConfigAction::GET:
		case dv::ConfigAction::GET_DESCRIPTION: {
			// Check parameters needed for operation.
			if (commandParts[CMD_PART_NODE] == nullptr) {
				std::cerr << "Error: missing node parameter." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_KEY] == nullptr) {
				std::cerr << "Error: missing key parameter." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_TYPE] == nullptr) {
				std::cerr << "Error: missing type parameter." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_TYPE + 1] != nullptr) {
				std::cerr << "Error: too many parameters for command." << std::endl;
				return;
			}

			auto type = dv::Config::Helper::stringToTypeConverter(commandParts[CMD_PART_TYPE]);
			if (type == dv::Config::AttributeType::UNKNOWN) {
				std::cerr << "Error: invalid type parameter." << std::endl;
				return;
			}

			auto nodeOffset = dataBufferSend.CreateString(commandParts[CMD_PART_NODE]);
			auto keyOffset  = dataBufferSend.CreateString(commandParts[CMD_PART_KEY]);

			msg = std::make_unique<dv::ConfigActionDataBuilder>(dataBufferSend);
			msg->add_action(action);
			msg->add_node(nodeOffset);
			msg->add_key(keyOffset);
			msg->add_type(static_cast<dv::ConfigType>(type));

			break;
		}

		case dv::ConfigAction::PUT: {
			// Check parameters needed for operation.
			if (commandParts[CMD_PART_NODE] == nullptr) {
				std::cerr << "Error: missing node parameter." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_KEY] == nullptr) {
				std::cerr << "Error: missing key parameter." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_TYPE] == nullptr) {
				std::cerr << "Error: missing type parameter." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_VALUE + 1] != nullptr) {
				std::cerr << "Error: too many parameters for command." << std::endl;
				return;
			}

			auto type = dv::Config::Helper::stringToTypeConverter(commandParts[CMD_PART_TYPE]);
			if (type == dv::Config::AttributeType::UNKNOWN) {
				std::cerr << "Error: invalid type parameter." << std::endl;
				return;
			}

			// Support setting STRING parameters to empty string.
			if (type != dv::Config::AttributeType::STRING) {
				if (commandParts[CMD_PART_VALUE] == nullptr) {
					std::cerr << "Error: missing value parameter." << std::endl;
					return;
				}
			}

			auto nodeOffset  = dataBufferSend.CreateString(commandParts[CMD_PART_NODE]);
			auto keyOffset   = dataBufferSend.CreateString(commandParts[CMD_PART_KEY]);
			auto valueOffset = dataBufferSend.CreateString(
				(commandParts[CMD_PART_VALUE] != nullptr) ? (commandParts[CMD_PART_VALUE]) : (""));

			msg = std::make_unique<dv::ConfigActionDataBuilder>(dataBufferSend);
			msg->add_action(action);
			msg->add_node(nodeOffset);
			msg->add_key(keyOffset);
			msg->add_type(static_cast<dv::ConfigType>(type));
			msg->add_value(valueOffset);

			break;
		}

		case dv::ConfigAction::ADD_MODULE: {
			// Check parameters needed for operation. Reuse node parameters.
			if (commandParts[CMD_PART_NODE] == nullptr) {
				std::cerr << "Error: missing module name." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_KEY] == nullptr) {
				std::cerr << "Error: missing library name." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_KEY + 1] != nullptr) {
				std::cerr << "Error: too many parameters for command." << std::endl;
				return;
			}

			auto nodeOffset = dataBufferSend.CreateString(commandParts[CMD_PART_NODE]);
			auto keyOffset  = dataBufferSend.CreateString(commandParts[CMD_PART_KEY]);

			msg = std::make_unique<dv::ConfigActionDataBuilder>(dataBufferSend);
			msg->add_action(action);
			msg->add_node(nodeOffset);
			msg->add_key(keyOffset);

			break;
		}

		case dv::ConfigAction::REMOVE_MODULE: {
			// Check parameters needed for operation. Reuse node parameters.
			if (commandParts[CMD_PART_NODE] == nullptr) {
				std::cerr << "Error: missing module name." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_NODE + 1] != nullptr) {
				std::cerr << "Error: too many parameters for command." << std::endl;
				return;
			}

			auto nodeOffset = dataBufferSend.CreateString(commandParts[CMD_PART_NODE]);

			msg = std::make_unique<dv::ConfigActionDataBuilder>(dataBufferSend);
			msg->add_action(action);
			msg->add_node(nodeOffset);

			break;
		}

		default:
			std::cerr << "Error: unknown command." << std::endl;
			return;
	}

	// Send formatted command to configuration server.
	try {
		sendMessage(std::move(msg));
	}
	catch (const boost::system::system_error &ex) {
		boost::format exMsg
			= boost::format("Unable to send data to config server, error message is:\n\t%s.") % ex.what();
		std::cerr << exMsg.str() << std::endl;
		return;
	}

	const dv::ConfigActionData *resp = nullptr;

	try {
		resp = receiveMessage();
	}
	catch (const boost::system::system_error &ex) {
		boost::format exMsg
			= boost::format("Unable to receive data from config server, error message is:\n\t%s.") % ex.what();
		std::cerr << exMsg.str() << std::endl;
		return;
	}

	if (action == dv::ConfigAction::DUMP_TREE) {
		while (true) {
			// Continue getting messages till finished.
			// End marked by confirmation message with same action.
			if (resp->action() == dv::ConfigAction::DUMP_TREE_NODE) {
				std::cout << "NODE: " << resp->node()->string_view() << std::endl;
			}
			else if (resp->action() == dv::ConfigAction::DUMP_TREE_ATTR) {
				std::cout << "ATTR: " << resp->node()->string_view() << " | " << resp->key()->string_view() << ", "
						  << dv::Config::Helper::typeToStringConverter(
								 static_cast<dv::Config::AttributeType>(resp->type()))
						  << " | " << resp->value()->string_view() << std::endl;
			}
			else if (resp->action() == dv::ConfigAction::DUMP_TREE) {
				// Done, confirmation received.
				break;
			}
			else {
				// Unexpected action.
				boost::format exMsg
					= boost::format("Unknown action '%s' during DUMP_TREE.") % dv::EnumNameConfigAction(resp->action());
				std::cerr << exMsg.str() << std::endl;
				return;
			}

			try {
				resp = receiveMessage();
			}
			catch (const boost::system::system_error &ex) {
				boost::format exMsg
					= boost::format("Unable to receive data from config server, error message is:\n\t%s.") % ex.what();
				std::cerr << exMsg.str() << std::endl;
				return;
			}
		}
	}

	// Display results.
	switch (resp->action()) {
		case dv::ConfigAction::ERROR:
			// Return error in 'value'.
			std::cerr << "ERROR on " << dv::EnumNameConfigAction(action) << ": " << resp->value()->string_view()
					  << std::endl;
			break;

		case dv::ConfigAction::NODE_EXISTS:
		case dv::ConfigAction::ATTR_EXISTS:
		case dv::ConfigAction::GET:
			// 'value' contains results in string format, use directly.
			std::cout << dv::EnumNameConfigAction(action) << ": " << resp->value()->string_view() << std::endl;
			break;

		case dv::ConfigAction::GET_DESCRIPTION:
			// Return help text in 'description'.
			std::cout << dv::EnumNameConfigAction(action) << ": " << resp->description()->string_view() << std::endl;
			break;

		case dv::ConfigAction::GET_CLIENT_ID:
			// Return 64bit client ID in 'id'.
			std::cout << dv::EnumNameConfigAction(action) << ": " << resp->id() << std::endl;
			break;

		default:
			// No return value, just action as confirmation.
			std::cout << dv::EnumNameConfigAction(action) << ": done" << std::endl;
			break;
	}
}

static void handleCommandCompletion(const char *buf, linenoiseCompletions *autoComplete) {
	const std::string commandStr(buf);

	// First let's split up the command into its constituents.
	std::string commandParts[MAX_CMD_PARTS + 1];

	// Split string into usable parts.
	boost::tokenizer<boost::char_separator<char>> cmdTokens(commandStr, boost::char_separator<char>(" "));

	size_t idx = 0;
	for (const auto &cmdTok : cmdTokens) {
		if (idx < MAX_CMD_PARTS) {
			commandParts[idx] = cmdTok;
		}
		else {
			// Abort, too many parts.
			return;
		}

		idx++;
	}

	// Also calculate number of commands already present in line (word-depth).
	// This is actually much more useful to understand where we are and what to do.
	size_t commandDepth = idx;

	if (commandDepth > 0 && !commandStr.empty() && commandStr.back() != ' ') {
		// If commands are present, ensure they have been "confirmed" by at least
		// one terminating spacing character. Else don't calculate the last command.
		commandDepth--;
	}

	// Check that we got something.
	if (commandDepth == 0) {
		// Always start off with a command/action.
		actionCompletion(commandStr, autoComplete, commandParts[CMD_PART_ACTION]);

		return;
	}

	// Let's get the action code first thing.
	dv::ConfigAction action = dv::ConfigAction::ERROR;

	for (const auto &act : actions) {
		if (act.name == commandParts[CMD_PART_ACTION]) {
			action = act.code;
		}
	}

	switch (action) {
		case dv::ConfigAction::NODE_EXISTS:
			if (commandDepth == 1) {
				nodeCompletion(commandStr, autoComplete, action, commandParts[CMD_PART_NODE]);
			}

			break;

		case dv::ConfigAction::ATTR_EXISTS:
		case dv::ConfigAction::GET:
		case dv::ConfigAction::GET_DESCRIPTION:
			if (commandDepth == 1) {
				nodeCompletion(commandStr, autoComplete, action, commandParts[CMD_PART_NODE]);
			}
			if (commandDepth == 2) {
				keyCompletion(
					commandStr, autoComplete, action, commandParts[CMD_PART_NODE], commandParts[CMD_PART_KEY]);
			}
			if (commandDepth == 3) {
				typeCompletion(commandStr, autoComplete, action, commandParts[CMD_PART_NODE],
					commandParts[CMD_PART_KEY], commandParts[CMD_PART_TYPE]);
			}

			break;

		case dv::ConfigAction::PUT:
			if (commandDepth == 1) {
				nodeCompletion(commandStr, autoComplete, action, commandParts[CMD_PART_NODE]);
			}
			if (commandDepth == 2) {
				keyCompletion(
					commandStr, autoComplete, action, commandParts[CMD_PART_NODE], commandParts[CMD_PART_KEY]);
			}
			if (commandDepth == 3) {
				typeCompletion(commandStr, autoComplete, action, commandParts[CMD_PART_NODE],
					commandParts[CMD_PART_KEY], commandParts[CMD_PART_TYPE]);
			}
			if (commandDepth == 4) {
				valueCompletion(commandStr, autoComplete, action, commandParts[CMD_PART_NODE],
					commandParts[CMD_PART_KEY], commandParts[CMD_PART_TYPE], commandParts[CMD_PART_VALUE]);
			}

			break;
	}
}

static void actionCompletion(
	const std::string &buf, linenoiseCompletions *autoComplete, const std::string &partialActionString) {
	UNUSED_ARGUMENT(buf);

	// Always start off with a command.
	for (const auto &act : actions) {
		if (partialActionString == act.name.substr(0, partialActionString.length())) {
			addCompletionSuffix(autoComplete, "", 0, act.name.c_str(), true, false);
		}
	}

	// Add quit and exit too.
	if (partialActionString == std::string("exit").substr(0, partialActionString.length())) {
		addCompletionSuffix(autoComplete, "", 0, "exit", true, false);
	}
	if (partialActionString == std::string("quit").substr(0, partialActionString.length())) {
		addCompletionSuffix(autoComplete, "", 0, "quit", true, false);
	}
}

static void nodeCompletion(const std::string &buf, linenoiseCompletions *autoComplete, dv::ConfigAction action,
	const std::string &partialNodeString) {
	UNUSED_ARGUMENT(action);

	// If partialNodeString is still empty, the first thing is to complete the root.
	if (partialNodeString.empty()) {
		addCompletionSuffix(autoComplete, buf.c_str(), buf.length(), "/", false, false);
		return;
	}

	// Get all the children of the last fully defined node (/ or /../../).
	std::string::size_type lastSlash = partialNodeString.rfind('/');
	if (lastSlash == std::string::npos) {
		// No / found, invalid, cannot auto-complete.
		return;
	}

	// Include slash character in size.
	lastSlash++;

	// Send request for all children names.
	auto nodeOffset = dataBufferSend.CreateString(partialNodeString.substr(0, lastSlash));

	auto msg = std::make_unique<dv::ConfigActionDataBuilder>(dataBufferSend);

	msg->add_action(dv::ConfigAction::GET_CHILDREN);
	msg->add_node(nodeOffset);

	try {
		sendMessage(std::move(msg));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	const dv::ConfigActionData *resp = nullptr;

	try {
		resp = receiveMessage();
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	if (resp->action() == dv::ConfigAction::ERROR) {
		// Invalid request made, no auto-completion.
		return;
	}

	// At this point we made a valid request and got back a full response.
	const auto respStr = resp->value()->str();
	boost::tokenizer<boost::char_separator<char>> children(respStr, boost::char_separator<char>("|"));

	size_t lengthOfIncompletePart = (partialNodeString.length() - lastSlash);

	for (const auto &child : children) {
		if (partialNodeString.substr(lastSlash, lengthOfIncompletePart) == child.substr(0, lengthOfIncompletePart)) {
			addCompletionSuffix(
				autoComplete, buf.c_str(), buf.length() - lengthOfIncompletePart, child.c_str(), false, true);
		}
	}
}

static void keyCompletion(const std::string &buf, linenoiseCompletions *autoComplete, dv::ConfigAction action,
	const std::string &nodeString, const std::string &partialKeyString) {
	UNUSED_ARGUMENT(action);

	// Send request for all attribute names for this node.
	auto nodeOffset = dataBufferSend.CreateString(nodeString);

	auto msg = std::make_unique<dv::ConfigActionDataBuilder>(dataBufferSend);

	msg->add_action(dv::ConfigAction::GET_ATTRIBUTES);
	msg->add_node(nodeOffset);

	try {
		sendMessage(std::move(msg));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	const dv::ConfigActionData *resp = nullptr;

	try {
		resp = receiveMessage();
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	if (resp->action() == dv::ConfigAction::ERROR) {
		// Invalid request made, no auto-completion.
		return;
	}

	// At this point we made a valid request and got back a full response.
	const auto respStr = resp->value()->str();
	boost::tokenizer<boost::char_separator<char>> attributes(respStr, boost::char_separator<char>("|"));

	for (const auto &attr : attributes) {
		if (partialKeyString == attr.substr(0, partialKeyString.length())) {
			addCompletionSuffix(
				autoComplete, buf.c_str(), buf.length() - partialKeyString.length(), attr.c_str(), true, false);
		}
	}
}

static void typeCompletion(const std::string &buf, linenoiseCompletions *autoComplete, dv::ConfigAction action,
	const std::string &nodeString, const std::string &keyString, const std::string &partialTypeString) {
	UNUSED_ARGUMENT(action);

	// Send request for the type name for this key on this node.
	auto nodeOffset = dataBufferSend.CreateString(nodeString);
	auto keyOffset  = dataBufferSend.CreateString(keyString);

	auto msg = std::make_unique<dv::ConfigActionDataBuilder>(dataBufferSend);

	msg->add_action(dv::ConfigAction::GET_TYPE);
	msg->add_node(nodeOffset);
	msg->add_key(keyOffset);

	try {
		sendMessage(std::move(msg));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	const dv::ConfigActionData *resp = nullptr;

	try {
		resp = receiveMessage();
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	if (resp->action() == dv::ConfigAction::ERROR) {
		// Invalid request made, no auto-completion.
		return;
	}

	// At this point we made a valid request and got back a full response.
	const std::string &typeStr
		= dv::Config::Helper::typeToStringConverter(static_cast<dv::Config::AttributeType>(resp->type()));
	if (partialTypeString == typeStr.substr(0, partialTypeString.length())) {
		addCompletionSuffix(
			autoComplete, buf.c_str(), buf.length() - partialTypeString.length(), typeStr.c_str(), true, false);
	}
}

static void valueCompletion(const std::string &buf, linenoiseCompletions *autoComplete, dv::ConfigAction action,
	const std::string &nodeString, const std::string &keyString, const std::string &typeString,
	const std::string &partialValueString) {
	UNUSED_ARGUMENT(action);

	auto type = dv::Config::Helper::stringToTypeConverter(typeString);
	if (type == dv::Config::AttributeType::UNKNOWN) {
		// Invalid type, no auto-completion.
		return;
	}

	if (!partialValueString.empty()) {
		// If there already is content, we can't do any auto-completion here, as
		// we have no idea about what a valid value would be to complete ...
		// Unless this is a boolean, then we can propose true/false strings.
		if (type == dv::Config::AttributeType::BOOL) {
			if (partialValueString == std::string("true").substr(0, partialValueString.length())) {
				addCompletionSuffix(
					autoComplete, buf.c_str(), buf.length() - partialValueString.length(), "true", false, false);
			}
			if (partialValueString == std::string("false").substr(0, partialValueString.length())) {
				addCompletionSuffix(
					autoComplete, buf.c_str(), buf.length() - partialValueString.length(), "false", false, false);
			}
		}

		return;
	}

	// Send request for the current value, so we can auto-complete with it as default.
	auto nodeOffset = dataBufferSend.CreateString(nodeString);
	auto keyOffset  = dataBufferSend.CreateString(keyString);

	auto msg = std::make_unique<dv::ConfigActionDataBuilder>(dataBufferSend);

	msg->add_action(dv::ConfigAction::GET);
	msg->add_node(nodeOffset);
	msg->add_key(keyOffset);
	msg->add_type(static_cast<dv::ConfigType>(type));

	try {
		sendMessage(std::move(msg));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	const dv::ConfigActionData *resp = nullptr;

	try {
		resp = receiveMessage();
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	if (resp->action() == dv::ConfigAction::ERROR) {
		// Invalid request made, no auto-completion.
		return;
	}

	// At this point we made a valid request and got back a full response.
	// We can just use it directly and paste it in as completion.
	addCompletionSuffix(autoComplete, buf.c_str(), buf.length(), resp->value()->c_str(), false, false);

	// If this is a boolean value, we can also add the inverse as a second completion.
	if (type == dv::Config::AttributeType::BOOL) {
		if (resp->value()->string_view() == "true") {
			addCompletionSuffix(autoComplete, buf.c_str(), buf.length(), "false", false, false);
		}
		else {
			addCompletionSuffix(autoComplete, buf.c_str(), buf.length(), "true", false, false);
		}
	}
}

static void addCompletionSuffix(linenoiseCompletions *autoComplete, const char *buf, size_t completionPoint,
	const char *suffix, bool endSpace, bool endSlash) {
	char concat[2048];

	if (endSpace) {
		if (endSlash) {
			snprintf(concat, 2048, "%.*s%s/ ", (int) completionPoint, buf, suffix);
		}
		else {
			snprintf(concat, 2048, "%.*s%s ", (int) completionPoint, buf, suffix);
		}
	}
	else {
		if (endSlash) {
			snprintf(concat, 2048, "%.*s%s/", (int) completionPoint, buf, suffix);
		}
		else {
			snprintf(concat, 2048, "%.*s%s", (int) completionPoint, buf, suffix);
		}
	}

	linenoiseAddCompletion(autoComplete, concat);
}
