#include "config.hpp"

#include "dv-sdk/cross/portable_io.h"

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace po = boost::program_options;

static boost::filesystem::path glConfigFile;

[[noreturn]] static inline void printHelpAndExit(po::options_description &desc) {
	std::cout << std::endl << desc << std::endl;
	exit(EXIT_FAILURE);
}

void dv::ConfigInit(int argc, char *argv[]) {
	// Allowed command-line options for configuration.
	po::options_description cliDescription("Command-line options");
	cliDescription.add_options()("help,h", "print help text")("config,c", po::value<std::string>(),
		"use the specified XML configuration file")("override,o", po::value<std::vector<std::string>>()->multitoken(),
		"override a configuration parameter from the XML configuration file with the supplied value.\n"
		"Format: <node> <attribute> <type> <value>\nExample: /system/logger/ logLevel byte 7");

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

	if (cliVarMap.count("override")) {
		// Always four components per override needed!
		if ((cliVarMap["override"].as<std::vector<std::string>>().size() % 4) != 0) {
			std::cout << "Configuration overrides must always have four components!" << std::endl;
			printHelpAndExit(cliDescription);
		}
	}

	if (cliVarMap.count("config")) {
		// User supplied config file.
		glConfigFile = boost::filesystem::path(cliVarMap["config"].as<std::string>());
	}
	else {
		// Default config file in $USER_HOME.
		char *userHome = portable_get_user_home_directory();
		glConfigFile   = boost::filesystem::path(std::string(userHome) + "/" + DV_CONFIG_FILE_NAME);
		free(userHome);
	}

	// Ensure file path is absolute.
	glConfigFile = boost::filesystem::absolute(glConfigFile);

	// Check that config file ends in .xml, exists, and is a normal file.
	if (glConfigFile.extension().string() != ".xml") {
		std::cout << "Supplied configuration file " << glConfigFile << " has no XML extension." << std::endl;
		printHelpAndExit(cliDescription);
	}

	if (boost::filesystem::exists(glConfigFile)) {
		if (!boost::filesystem::is_regular_file(glConfigFile)) {
			std::cout << "Supplied configuration file " << glConfigFile << " could not be accessed." << std::endl;
			printHelpAndExit(cliDescription);
		}
	}
	else {
		// File doesn't exist yet, let's make sure the parent directory
		// at least exists and is a directory.
		if (!boost::filesystem::is_directory(glConfigFile.parent_path())) {
			std::cout << "Supplied configuration file directory " << glConfigFile.parent_path()
					  << " could not be accessed." << std::endl;
			printHelpAndExit(cliDescription);
		}
	}

	// Let's try to open the file for reading, or create it.
	int configFileFd = open(glConfigFile.string().c_str(), O_RDONLY | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);

	if (configFileFd >= 0) {
		// File opened for reading (or created) successfully.
		// Load XML configuration from file if not empty.
		struct stat configFileStat;
		fstat(configFileFd, &configFileStat);

		if (configFileStat.st_size > 0) {
			dv::Config::GLOBAL.getRootNode().importSubTreeFromXML(configFileFd, true);
		}

		close(configFileFd);

		// This means it exists and we can access it, so let's remember
		// it for writing the configuration later at shutdown.
		glConfigFile = boost::filesystem::canonical(glConfigFile);
	}
	else {
		std::cout << "Supplied configuration file " << glConfigFile
				  << " could not be created or read. Error: " << strerror(errno) << "." << std::endl;
		printHelpAndExit(cliDescription);
	}

	// Override with command-line arguments if requested.
	if (cliVarMap.count("override")) {
		std::vector<std::string> configOverrides = cliVarMap["override"].as<std::vector<std::string>>();

		auto iter = configOverrides.begin();

		while (iter != configOverrides.end()) {
			// Get node.
			try {
				auto node = dv::Config::GLOBAL.getNode(iter[0]);

				if (!node.stringToAttributeConverter(iter[1], iter[2], iter[3], true)) {
					std::cout << "Config: failed to convert attribute '" << iter[1] << "' of type '" << iter[2]
							  << "' with value '" << iter[3] << "' on override." << std::endl;
				}
			}
			catch (const std::out_of_range &) {
				std::cout << "Config: invalid node path specification '" << iter[0] << "' on override." << std::endl;
			}

			iter += 4;
		}
	}
}

void dv::ConfigWriteBack() {
	// configFile can only be correctly initialized, absolute and canonical
	// by the point this function may ever be called, so we use it directly.
	int configFileFd = open(glConfigFile.string().c_str(), O_WRONLY | O_TRUNC);

	if (configFileFd >= 0) {
		dv::Config::GLOBAL.getRootNode().exportSubTreeToXML(configFileFd);

		portable_fsync(configFileFd);
		close(configFileFd);

		caerLog(CAER_LOG_DEBUG, "Config", "Configuration file '%s' written to disk.", glConfigFile.string().c_str());
	}
	else {
		caerLog(CAER_LOG_EMERGENCY, "Config", "Could not write to the configuration file '%s'. Error: %d.",
			glConfigFile.string().c_str(), errno);
	}
}

void dv::ConfigWriteBackListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL
		&& caerStrEquals(changeKey, "writeConfiguration") && changeValue.boolean) {
		ConfigWriteBack();

		dvConfigNodeAttributeButtonReset(node, changeKey);
	}
}
