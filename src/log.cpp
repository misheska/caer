#include "log.hpp"

#include "dv-sdk/cross/portable_io.h"

#include <boost/filesystem.hpp>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

static int DV_LOG_FILE_FD  = -1;
static dvCfg::Node logNode = nullptr;

static void logMessagesToConfigTree(const char *msg, size_t msgLength);
static void logShutDownWriteBack(void);
static void logConfigLogger(const char *msg, bool fatal);
static void logLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

void dv::LoggerInit(void) {
	logNode = dvCfg::Tree::globalTree().getNode("/system/logger/");

	// Ensure default log file and value are present.
	char *userHome                       = portable_get_user_home_directory();
	const std::string logFileDefaultPath = std::string(userHome) + "/" + DV_LOG_FILE_NAME;
	free(userHome);

	logNode.create<dvCfgType::STRING>("logFile", logFileDefaultPath, {2, PATH_MAX}, dvCfgFlags::NORMAL,
		"Path to the file where all log messages are written to.");

	logNode.create<dvCfgType::INT>(
		"logLevel", CAER_LOG_NOTICE, {CAER_LOG_EMERGENCY, CAER_LOG_DEBUG}, dvCfgFlags::NORMAL, "Global log-level.");

	logNode.create<dvCfgType::STRING>("lastLogMessage", "Logging initialized.", {0, 32 * 1024},
		dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Path to the file where all log messages are written to.");

	// Try to open the specified file and error out if not possible.
	const std::string logFile = logNode.get<dvCfgType::STRING>("logFile");
	DV_LOG_FILE_FD            = open(logFile.c_str(), O_WRONLY | O_APPEND | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);

	if (DV_LOG_FILE_FD < 0) {
		// Must be able to open log file! _REQUIRED_
		caerLog(CAER_LOG_EMERGENCY, "Logger", "Failed to open log file '%s'. Error: %d.", logFile.c_str(), errno);

		exit(EXIT_FAILURE);
	}

	// Set global log level and install listener for its update.
	int32_t logLevel = logNode.get<dvCfgType::INT>("logLevel");
	caerLogLevelSet(static_cast<enum caer_log_level>(logLevel));

	logNode.addAttributeListener(nullptr, &logLevelListener);

	// Switch log messages to log file and stderr.
	caerLogFileDescriptorsSet(DV_LOG_FILE_FD, STDERR_FILENO);

	// Make sure log file gets flushed at exit time.
	atexit(&logShutDownWriteBack);

	// Send any log messages out via ConfigTree from now on.
	caerLogCallbackSet(&logMessagesToConfigTree);

	// Now that config is initialized (has to be!) and logging too, we can
	// set the ConfigTree logger to use our internal logger too.
	dvConfigTreeErrorLogCallbackSet(&logConfigLogger);

	// Log sub-system initialized fully and correctly, log this.
	caerLog(CAER_LOG_DEBUG, "Logger", "Started with log file '%s', log-level %d.", logFile.c_str(), logLevel);
}

static void logMessagesToConfigTree(const char *msg, size_t msgLength) {
	dvConfigAttributeValue logMessage;
	logMessage.string = const_cast<char *>(msg);

	// Remove trailing newline (replace with NUL terminator).
	// HACK: this works by bypassing const on the input message.
	// We do know this is fine due to caerLog() putting msg in RW memory
	// and passing it to the callback last by design.
	logMessage.string[msgLength - 1] = '\0';

	logNode.updateReadOnlyAttribute("lastLogMessage", dvCfgType::STRING, logMessage);
}

static void logShutDownWriteBack(void) {
	caerLog(CAER_LOG_DEBUG, "Logger", "Shutting down, flushing outputs.");

	// Flush interactive outputs.
	fflush(stdout);
	fflush(stderr);

	// Ensure proper flushing and closing of the log file at shutdown.
	portable_fsync(DV_LOG_FILE_FD);
	close(DV_LOG_FILE_FD);
}

static void logConfigLogger(const char *msg, bool fatal) {
	if (fatal) {
		throw std::runtime_error(msg);
	}
	else {
		caerLog(CAER_LOG_ERROR, "Config", "%s", msg);
	}
}

static void logLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);
	UNUSED_ARGUMENT(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "logLevel")) {
		// Update the global log level asynchronously.
		caerLogLevelSet(static_cast<enum caer_log_level>(changeValue.iint));
		caerLog(CAER_LOG_DEBUG, "Logger", "Log-level set to %d.", changeValue.iint);
	}
}
