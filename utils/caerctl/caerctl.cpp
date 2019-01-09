#include "caer-sdk/cross/portable_io.h"
#include "caer-sdk/utils.h"

#include "src/config_server/config_action_data.h"
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

#define CAERCTL_HISTORY_FILE_NAME ".caer-ctl.history"

static void handleInputLine(const char *buf, size_t bufLength);
static void handleCommandCompletion(const char *buf, linenoiseCompletions *autoComplete);

static void actionCompletion(
	const std::string &buf, linenoiseCompletions *autoComplete, const std::string &partialActionString);
static void nodeCompletion(const std::string &buf, linenoiseCompletions *autoComplete, caer_config_actions action,
	const std::string &partialNodeString);
static void keyCompletion(const std::string &buf, linenoiseCompletions *autoComplete, caer_config_actions action,
	const std::string &nodeString, const std::string &partialKeyString);
static void typeCompletion(const std::string &buf, linenoiseCompletions *autoComplete, caer_config_actions action,
	const std::string &nodeString, const std::string &keyString, const std::string &partialTypeString);
static void valueCompletion(const std::string &buf, linenoiseCompletions *autoComplete, caer_config_actions action,
	const std::string &nodeString, const std::string &keyString, const std::string &typeString,
	const std::string &partialValueString);
static void addCompletionSuffix(linenoiseCompletions *lc, const char *buf, size_t completionPoint, const char *suffix,
	bool endSpace, bool endSlash);

static const struct {
	const std::string name;
	const caer_config_actions code;
} actions[] = {
	{"node_exists", caer_config_actions::CAER_CONFIG_NODE_EXISTS},
	{"attr_exists", caer_config_actions::CAER_CONFIG_ATTR_EXISTS},
	{"get", caer_config_actions::CAER_CONFIG_GET},
	{"put", caer_config_actions::CAER_CONFIG_PUT},
	{"help", caer_config_actions::CAER_CONFIG_GET_DESCRIPTION},
	{"add_module", caer_config_actions::CAER_CONFIG_ADD_MODULE},
	{"remove_module", caer_config_actions::CAER_CONFIG_REMOVE_MODULE},
};
static const size_t actionsLength = sizeof(actions) / sizeof(actions[0]);

static ConfigActionData dataBuffer;

static asio::io_service ioService;
static asioSSL::context sslContext(asioSSL::context::tlsv12_client);
static asioSSL::stream<asioTCP::socket> sslSocket(ioService, sslContext);
static bool sslConnection = false;

[[noreturn]] static inline void printHelpAndExit(po::options_description &desc) {
	std::cout << std::endl << desc << std::endl;
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
	// Allowed command-line options for caer-ctl.
	po::options_description cliDescription("Command-line options");
	cliDescription.add_options()("help,h", "print help text")("ipaddress,i", po::value<std::string>(),
		"IP-address or hostname to connect to")("port,p", po::value<std::string>(), "port to connect to")("ssl",
		po::value<std::string>()->implicit_value(""),
		"enable SSL for connection (no argument uses default CA for verification, or pass a path to a specific CA file "
		"in the PEM format)")(
		"sslcert", po::value<std::string>(), "SSL certificate file for client authentication (PEM format)")(
		"sslkey", po::value<std::string>(), "SSL key file for client authentication (PEM format)")("script,s",
		po::value<std::vector<std::string>>()->multitoken(),
		"script mode, sends the given command directly to the server as if typed in and exits.\n"
		"Format: <action> <node> [<attribute> <type> [<value>]]\nExample: set /caer/logger/ logLevel byte 7");

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

	if (cliVarMap.count("ssl")) {
		sslConnection = true;

		// Client-side SSL authentication support.
		if (cliVarMap.count("sslcert")) {
			std::string sslCertFile = cliVarMap["sslcert"].as<std::string>();

			try {
				sslContext.use_certificate_chain_file(sslCertFile);
			}
			catch (const boost::system::system_error &ex) {
				boost::format exMsg
					= boost::format("Failed to load SSL client certificate file '%s', error message is:\n\t%s.")
					  % sslCertFile % ex.what();
				std::cerr << exMsg.str() << std::endl;
				return (EXIT_FAILURE);
			}
		}

		if (cliVarMap.count("sslkey")) {
			std::string sslKeyFile = cliVarMap["sslkey"].as<std::string>();

			try {
				sslContext.use_private_key_file(sslKeyFile, boost::asio::ssl::context::pem);
			}
			catch (const boost::system::system_error &ex) {
				boost::format exMsg = boost::format("Failed to load SSL client key file '%s', error message is:\n\t%s.")
									  % sslKeyFile % ex.what();
				std::cerr << exMsg.str() << std::endl;
				return (EXIT_FAILURE);
			}
		}

		sslContext.set_options(
			boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::single_dh_use);

		std::string sslVerifyFile = cliVarMap["ssl"].as<std::string>();

		if (sslVerifyFile.empty()) {
			sslContext.set_default_verify_paths();
		}
		else {
			try {
				sslContext.load_verify_file(sslVerifyFile);
			}
			catch (const boost::system::system_error &ex) {
				boost::format exMsg
					= boost::format("Failed to load SSL CA verification file '%s', error message is:\n\t%s.")
					  % sslVerifyFile % ex.what();
				std::cerr << exMsg.str() << std::endl;
				return (EXIT_FAILURE);
			}
		}

		sslContext.set_verify_mode(asioSSL::context::verify_peer);

		// Rebuild SSL socket, so it picks up changes to SSL context above.
		new (&sslSocket) asioSSL::stream<asioTCP::socket>(ioService, sslContext);
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

	commandHistoryFilePath.append(CAERCTL_HISTORY_FILE_NAME, boost::filesystem::path::codecvt());

	// Connect to the remote cAER config server.
	try {
		asioTCP::resolver resolver(ioService);
		asio::connect(sslSocket.next_layer(), resolver.resolve({ipAddress, portNumber}));
	}
	catch (const boost::system::system_error &ex) {
		boost::format exMsg = boost::format("Failed to connect to %s:%s, error message is:\n\t%s.") % ipAddress
							  % portNumber % ex.what();
		std::cerr << exMsg.str() << std::endl;
		return (EXIT_FAILURE);
	}

	// SSL connection support.
	if (sslConnection) {
		try {
			sslSocket.handshake(asioSSL::stream_base::client);
		}
		catch (const boost::system::system_error &ex) {
			boost::format exMsg = boost::format("Failed SSL handshake, error message is:\n\t%s.") % ex.what();
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
		boost::format shellPrompt = boost::format("cAER @ %s:%s >> ") % ipAddress % portNumber;

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

	// Close SSL connection properly.
	if (sslConnection) {
		boost::system::error_code error;
		sslSocket.shutdown(error);

		// EOF is expected for good SSL shutdown. See:
		// https://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
		if (error != asio::error::eof) {
			boost::format errMsg = boost::format("Failed SSL shutdown, error message is:\n\t%s.") % error.message();
			std::cerr << errMsg.str() << std::endl;
		}
	}

	return (EXIT_SUCCESS);
}

static inline void asioSocketWrite(const asio::const_buffer &buf) {
	const asio::const_buffers_1 buf2(buf);

	if (sslConnection) {
		asio::write(sslSocket, buf2);
	}
	else {
		asio::write(sslSocket.next_layer(), buf2);
	}
}

static inline void asioSocketRead(const asio::mutable_buffer &buf) {
	const asio::mutable_buffers_1 buf2(buf);

	if (sslConnection) {
		asio::read(sslSocket, buf2);
	}
	else {
		asio::read(sslSocket.next_layer(), buf2);
	}
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
	caer_config_actions action;

	for (size_t i = 0; i < actionsLength; i++) {
		if (actions[i].name == commandParts[CMD_PART_ACTION]) {
			action = actions[i].code;
		}
	}

	// Now that we know what we want to do, let's decode the command line.
	switch (action) {
		case caer_config_actions::CAER_CONFIG_NODE_EXISTS: {
			// Check parameters needed for operation.
			if (commandParts[CMD_PART_NODE] == nullptr) {
				std::cerr << "Error: missing node parameter." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_NODE + 1] != nullptr) {
				std::cerr << "Error: too many parameters for command." << std::endl;
				return;
			}

			dataBuffer.reset();
			dataBuffer.setAction(action);
			dataBuffer.setNode(commandParts[CMD_PART_NODE]);

			break;
		}

		case caer_config_actions::CAER_CONFIG_ATTR_EXISTS:
		case caer_config_actions::CAER_CONFIG_GET:
		case caer_config_actions::CAER_CONFIG_GET_DESCRIPTION: {
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

			enum sshs_node_attr_value_type type = sshsHelperStringToTypeConverter(commandParts[CMD_PART_TYPE]);
			if (type == SSHS_UNKNOWN) {
				std::cerr << "Error: invalid type parameter." << std::endl;
				return;
			}

			dataBuffer.reset();
			dataBuffer.setAction(action);
			dataBuffer.setType(type);
			dataBuffer.setNode(commandParts[CMD_PART_NODE]);
			dataBuffer.setKey(commandParts[CMD_PART_KEY]);

			break;
		}

		case caer_config_actions::CAER_CONFIG_PUT: {
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
			if (commandParts[CMD_PART_VALUE] == nullptr) {
				std::cerr << "Error: missing value parameter." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_VALUE + 1] != nullptr) {
				std::cerr << "Error: too many parameters for command." << std::endl;
				return;
			}

			enum sshs_node_attr_value_type type = sshsHelperStringToTypeConverter(commandParts[CMD_PART_TYPE]);
			if (type == SSHS_UNKNOWN) {
				std::cerr << "Error: invalid type parameter." << std::endl;
				return;
			}

			dataBuffer.reset();
			dataBuffer.setAction(action);
			dataBuffer.setType(type);
			dataBuffer.setNode(commandParts[CMD_PART_NODE]);
			dataBuffer.setKey(commandParts[CMD_PART_KEY]);
			dataBuffer.setValue(commandParts[CMD_PART_VALUE]);

			break;
		}

		case caer_config_actions::CAER_CONFIG_ADD_MODULE: {
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

			dataBuffer.reset();
			dataBuffer.setAction(action);
			dataBuffer.setNode(commandParts[CMD_PART_NODE]);
			dataBuffer.setKey(commandParts[CMD_PART_KEY]);

			break;
		}

		case caer_config_actions::CAER_CONFIG_REMOVE_MODULE: {
			// Check parameters needed for operation. Reuse node parameters.
			if (commandParts[CMD_PART_NODE] == nullptr) {
				std::cerr << "Error: missing module name." << std::endl;
				return;
			}
			if (commandParts[CMD_PART_NODE + 1] != nullptr) {
				std::cerr << "Error: too many parameters for command." << std::endl;
				return;
			}

			dataBuffer.reset();
			dataBuffer.setAction(action);
			dataBuffer.setNode(commandParts[CMD_PART_NODE]);

			break;
		}

		default:
			std::cerr << "Error: unknown command." << std::endl;
			return;
	}

	// Send formatted command to configuration server.
	try {
		asioSocketWrite(asio::buffer(dataBuffer.getBuffer(), dataBuffer.size()));
	}
	catch (const boost::system::system_error &ex) {
		boost::format exMsg
			= boost::format("Unable to send data to config server, error message is:\n\t%s.") % ex.what();
		std::cerr << exMsg.str() << std::endl;
		return;
	}

	try {
		asioSocketRead(asio::buffer(dataBuffer.getHeaderBuffer(), dataBuffer.headerSize()));
	}
	catch (const boost::system::system_error &ex) {
		boost::format exMsg
			= boost::format("Unable to receive data from config server, error message is:\n\t%s.") % ex.what();
		std::cerr << exMsg.str() << std::endl;
		return;
	}

	// Total length to get for response.
	try {
		asioSocketRead(asio::buffer(dataBuffer.getDataBuffer(), dataBuffer.dataSize()));
	}
	catch (const boost::system::system_error &ex) {
		boost::format exMsg
			= boost::format("Unable to receive data from config server, error message is:\n\t%s.") % ex.what();
		std::cerr << exMsg.str() << std::endl;
		return;
	}

	// Convert action back to a string.
	std::string actionString;

	// Detect error response.
	if (dataBuffer.getAction() == caer_config_actions::CAER_CONFIG_ERROR) {
		actionString = "error";
	}
	else {
		for (size_t i = 0; i < actionsLength; i++) {
			if (actions[i].code == dataBuffer.getAction()) {
				actionString = actions[i].name;
			}
		}
	}

	// Display results.
	boost::format resultMsg = boost::format("Result: action=%s, type=%s, msgLength=%" PRIu16 ", msg='%s'.")
							  % actionString % sshsHelperTypeToStringConverter(dataBuffer.getType())
							  % dataBuffer.getNodeLength() % dataBuffer.getNode();

	std::cout << resultMsg.str() << std::endl;
}

static void handleCommandCompletion(const char *buf, linenoiseCompletions *autoComplete) {
	std::string commandStr(buf);

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
	caer_config_actions action;

	for (size_t i = 0; i < actionsLength; i++) {
		if (actions[i].name == commandParts[CMD_PART_ACTION]) {
			action = actions[i].code;
		}
	}

	switch (action) {
		case caer_config_actions::CAER_CONFIG_NODE_EXISTS:
			if (commandDepth == 1) {
				nodeCompletion(commandStr, autoComplete, action, commandParts[CMD_PART_NODE]);
			}

			break;

		case caer_config_actions::CAER_CONFIG_ATTR_EXISTS:
		case caer_config_actions::CAER_CONFIG_GET:
		case caer_config_actions::CAER_CONFIG_GET_DESCRIPTION:
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

		case caer_config_actions::CAER_CONFIG_PUT:
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
	for (size_t i = 0; i < actionsLength; i++) {
		if (partialActionString == actions[i].name.substr(0, partialActionString.length())) {
			addCompletionSuffix(autoComplete, "", 0, actions[i].name.c_str(), true, false);
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

static void nodeCompletion(const std::string &buf, linenoiseCompletions *autoComplete, caer_config_actions action,
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
	dataBuffer.reset();
	dataBuffer.setAction(caer_config_actions::CAER_CONFIG_GET_CHILDREN);
	dataBuffer.setNode(std::string(partialNodeString, lastSlash));

	try {
		asioSocketWrite(asio::buffer(dataBuffer.getBuffer(), dataBuffer.size()));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	try {
		asioSocketRead(asio::buffer(dataBuffer.getHeaderBuffer(), dataBuffer.headerSize()));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	// Total length to get for response.
	try {
		asioSocketRead(asio::buffer(dataBuffer.getDataBuffer(), dataBuffer.dataSize()));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	if (dataBuffer.getAction() == caer_config_actions::CAER_CONFIG_ERROR || dataBuffer.getType() != SSHS_STRING) {
		// Invalid request made, no auto-completion.
		return;
	}

	// At this point we made a valid request and got back a full response.
	boost::tokenizer<boost::char_separator<char>> nodeChildren(dataBuffer.getNode(), boost::char_separator<char>("\0"));

	for (const auto &child : nodeChildren) {
		size_t lengthOfIncompletePart = (partialNodeString.length() - lastSlash);

		if (partialNodeString.substr(lastSlash, lengthOfIncompletePart) == child.substr(0, lengthOfIncompletePart)) {
			addCompletionSuffix(
				autoComplete, buf.c_str(), buf.length() - lengthOfIncompletePart, child.c_str(), false, true);
		}
	}
}

static void keyCompletion(const std::string &buf, linenoiseCompletions *autoComplete, caer_config_actions action,
	const std::string &nodeString, const std::string &partialKeyString) {
	UNUSED_ARGUMENT(action);

	// Send request for all attribute names for this node.
	dataBuffer.reset();
	dataBuffer.setAction(caer_config_actions::CAER_CONFIG_GET_ATTRIBUTES);
	dataBuffer.setNode(nodeString);

	try {
		asioSocketWrite(asio::buffer(dataBuffer.getBuffer(), dataBuffer.size()));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	try {
		asioSocketRead(asio::buffer(dataBuffer.getHeaderBuffer(), dataBuffer.headerSize()));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	// Total length to get for response.
	try {
		asioSocketRead(asio::buffer(dataBuffer.getDataBuffer(), dataBuffer.dataSize()));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	if (dataBuffer.getAction() == caer_config_actions::CAER_CONFIG_ERROR || dataBuffer.getType() != SSHS_STRING) {
		// Invalid request made, no auto-completion.
		return;
	}

	// At this point we made a valid request and got back a full response.
	boost::tokenizer<boost::char_separator<char>> attributes(dataBuffer.getNode(), boost::char_separator<char>("\0"));

	for (const auto &attr : attributes) {
		if (partialKeyString == attr.substr(0, partialKeyString.length())) {
			addCompletionSuffix(
				autoComplete, buf.c_str(), buf.length() - partialKeyString.length(), attr.c_str(), true, false);
		}
	}
}

static void typeCompletion(const std::string &buf, linenoiseCompletions *autoComplete, caer_config_actions action,
	const std::string &nodeString, const std::string &keyString, const std::string &partialTypeString) {
	UNUSED_ARGUMENT(action);

	// Send request for the type name for this key on this node.
	dataBuffer.reset();
	dataBuffer.setAction(caer_config_actions::CAER_CONFIG_GET_TYPE);
	dataBuffer.setNode(nodeString);
	dataBuffer.setKey(keyString);

	try {
		asioSocketWrite(asio::buffer(dataBuffer.getBuffer(), dataBuffer.size()));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	try {
		asioSocketRead(asio::buffer(dataBuffer.getHeaderBuffer(), dataBuffer.headerSize()));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	// Total length to get for response.
	try {
		asioSocketRead(asio::buffer(dataBuffer.getDataBuffer(), dataBuffer.dataSize()));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	if (dataBuffer.getAction() == caer_config_actions::CAER_CONFIG_ERROR || dataBuffer.getType() != SSHS_STRING) {
		// Invalid request made, no auto-completion.
		return;
	}

	// At this point we made a valid request and got back a full response.
	if (partialTypeString == dataBuffer.getNode().substr(0, partialTypeString.length())) {
		addCompletionSuffix(autoComplete, buf.c_str(), buf.length() - partialTypeString.length(),
			dataBuffer.getNode().c_str(), true, false);
	}
}

static void valueCompletion(const std::string &buf, linenoiseCompletions *autoComplete, caer_config_actions action,
	const std::string &nodeString, const std::string &keyString, const std::string &typeString,
	const std::string &partialValueString) {
	UNUSED_ARGUMENT(action);

	enum sshs_node_attr_value_type type = sshsHelperStringToTypeConverter(typeString.c_str());
	if (type == SSHS_UNKNOWN) {
		// Invalid type, no auto-completion.
		return;
	}

	if (!partialValueString.empty()) {
		// If there already is content, we can't do any auto-completion here, as
		// we have no idea about what a valid value would be to complete ...
		// Unless this is a boolean, then we can propose true/false strings.
		if (type == SSHS_BOOL) {
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
	dataBuffer.reset();
	dataBuffer.setAction(caer_config_actions::CAER_CONFIG_GET);
	dataBuffer.setType(type);
	dataBuffer.setNode(nodeString);
	dataBuffer.setKey(keyString);

	try {
		asioSocketWrite(asio::buffer(dataBuffer.getBuffer(), dataBuffer.size()));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	try {
		asioSocketRead(asio::buffer(dataBuffer.getHeaderBuffer(), dataBuffer.headerSize()));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	// Total length to get for response.
	try {
		asioSocketRead(asio::buffer(dataBuffer.getDataBuffer(), dataBuffer.dataSize()));
	}
	catch (const boost::system::system_error &) {
		// Failed to contact remote host, no auto-completion!
		return;
	}

	if (dataBuffer.getAction() == caer_config_actions::CAER_CONFIG_ERROR) {
		// Invalid request made, no auto-completion.
		return;
	}

	// At this point we made a valid request and got back a full response.
	// We can just use it directly and paste it in as completion.
	addCompletionSuffix(autoComplete, buf.c_str(), buf.length(), dataBuffer.getNode().c_str(), false, false);

	// If this is a boolean value, we can also add the inverse as a second completion.
	if (type == SSHS_BOOL) {
		if (dataBuffer.getNode() == "true") {
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
