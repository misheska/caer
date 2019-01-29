#include "caer-sdk/cross/portable_io.h"
#include "caer-sdk/cross/portable_threads.h"
#include "caer-sdk/utils.h"

#include "../../src/config_server/asio.h"
#include "../../src/config_server/dv_config_action_data.h"
#include "utils/ext/linenoise-ng/linenoise.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace po = boost::program_options;

#define CAERCTL_HISTORY_FILE_NAME ".caer-ctl.history"

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
};

#define CAER_CONFIG_CLIENT_MAX_INCOMING_SIZE 8192

static asio::io_service ioService;
static asioSSL::context sslContext(asioSSL::context::tlsv12_client);
static bool sslConnection = false;

[[noreturn]] static inline void printHelpAndExit(const po::options_description &desc) {
	std::cout << std::endl << desc << std::endl;
	exit(EXIT_FAILURE);
}

template<typename MsgOps>
static inline void sendMessage(std::shared_ptr<TCPTLSWriteOrderedSocket> sock, MsgOps msgFunc) {
	// Send back flags directly.
	auto msgBuild = std::make_shared<flatbuffers::FlatBufferBuilder>(CAER_CONFIG_CLIENT_MAX_INCOMING_SIZE);

	dv::ConfigActionDataBuilder msg(*msgBuild);

	msgFunc(msg);

	// Finish off message.
	auto msgRoot = msg.Finish();

	// Write root node and message size.
	dv::FinishSizePrefixedConfigActionDataBuffer(*msgBuild, msgRoot);

	sock->write(asio::buffer(msgBuild->GetBufferPointer(), msgBuild->GetSize()),
		[sock, msgBuild](const boost::system::error_code &error, size_t /*length*/) {
			if (error) {
				std::cout << "Failed to write message" << std::endl;
			}
		});
}

class ConfigIOHandler {
private:
	std::shared_ptr<TCPTLSWriteOrderedSocket> sock;
	std::thread ioThread;
	uint64_t clientId;

public:
	explicit ConfigIOHandler(std::shared_ptr<TCPTLSWriteOrderedSocket> s) : sock(std::move(s)) {
	}

	void threadStart() {
		ioThread = std::thread([this]() {
			// Set thread name.
			portable_thread_set_name("ConfigIOHandler");

			// Initialize connection.
			sock->start(
				[this](const boost::system::error_code &error) {
					if (error) {
						std::cout << "Failed to start connection (SSL handshake failure)." << std::endl;
					}
					else {
						startPushClient();
					}
				},
				asioSSL::stream_base::client);

			// Run IO service.
			ioService.run();
		});
	}

	void threadStop() {
		// Stop IO service.
		ioService.stop();

		// Wait for thread to terminate.
		ioThread.join();
	}

	void startPushClient() {
		// Get client ID from remote.
		sendMessage(sock, [](dv::ConfigActionDataBuilder &msg) { msg.add_action(dv::ConfigAction::GET_CLIENT_ID); });
		receiveMessage();
	}

	void receiveMessage() {
		auto response = std::shared_ptr<uint8_t[]>(new uint8_t[CAER_CONFIG_CLIENT_MAX_INCOMING_SIZE]);

		sock->read(asio::buffer(response.get(), 4), [this, response](const boost::system::error_code &, size_t) {
			uint32_t size = le32toh(*reinterpret_cast<uint32_t *>(response.get()));
			std::cout << "SIZE IS " << size << std::endl;

			sock->read(asio::buffer(response.get(), size), [this, response](const boost::system::error_code &, size_t) {
				auto configData = dv::GetConfigActionData(response.get());
				std::cout << "ID IS " << configData->id() << std::endl;
			});
		});
	}
};

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
	std::shared_ptr<TCPTLSWriteOrderedSocket> netSocket;

	try {
		asioTCP::socket sock{ioService};
		asioTCP::resolver resolver{ioService};
		asio::connect(sock, resolver.resolve({ipAddress, portNumber}));

		// SSL connection support.
		netSocket = std::make_shared<TCPTLSWriteOrderedSocket>(std::move(sock), sslConnection, &sslContext);
	}
	catch (const boost::system::system_error &ex) {
		boost::format exMsg = boost::format("Failed to connect to %s:%s, error message is:\n\t%s.") % ipAddress
							  % portNumber % ex.what();
		std::cerr << exMsg.str() << std::endl;
		return (EXIT_FAILURE);
	}

	ConfigIOHandler ioThread(netSocket);

	ioThread.threadStart();

	std::this_thread::sleep_for(std::chrono::seconds(2));

	ioThread.threadStop();
}
