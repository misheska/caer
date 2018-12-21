#include "log.h"

#include "caer-sdk/cross/portable_io.h"

#include <boost/filesystem.hpp>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int CAER_LOG_FILE_FD = -1;

static void caerLogShutDownWriteBack(void);
static void caerLogSSHSLogger(const char *msg, bool fatal);
static void caerLogMessagesToSSHS(const char *msg, size_t msgLength);
static void caerLogLevelListener(sshsNode node, void *userData, enum sshs_node_attribute_events event,
	const char *changeKey, enum sshs_node_attr_value_type changeType, union sshs_node_attr_value changeValue);

void caerLogInit(void) {
	sshsNode logNode = sshsGetNode(sshsGetGlobal(), "/caer/logger/");

	// Ensure default log file and value are present.
	char *userHome                       = portable_get_user_home_directory();
	const std::string logFileDefaultPath = std::string(userHome) + "/" + CAER_LOG_FILE_NAME;
	free(userHome);

	sshsNodeCreate(logNode, "logFile", logFileDefaultPath, 2, PATH_MAX, SSHS_FLAGS_NORMAL,
		"Path to the file where all log messages are written to.");

	sshsNodeCreateInt(logNode, "logLevel", CAER_LOG_NOTICE, CAER_LOG_EMERGENCY, CAER_LOG_DEBUG, SSHS_FLAGS_NORMAL,
		"Global log-level.");

	sshsNodeCreate(logNode, "lastLogMessage", "Logging initialized.", 0, 32 * 1024,
		SSHS_FLAGS_READ_ONLY | SSHS_FLAGS_NO_EXPORT, "Path to the file where all log messages are written to.");

	// Try to open the specified file and error out if not possible.
	const std::string logFile = sshsNodeGetStdString(logNode, "logFile");
	CAER_LOG_FILE_FD          = open(logFile.c_str(), O_WRONLY | O_APPEND | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);

	if (CAER_LOG_FILE_FD < 0) {
		// Must be able to open log file! _REQUIRED_
		caerLog(CAER_LOG_EMERGENCY, "Logger", "Failed to open log file '%s'. Error: %d.", logFile.c_str(), errno);

		exit(EXIT_FAILURE);
	}

	// Set global log level and install listener for its update.
	int32_t logLevel = sshsNodeGetInt(logNode, "logLevel");
	caerLogLevelSet((enum caer_log_level) logLevel);

	sshsNodeAddAttributeListener(logNode, nullptr, &caerLogLevelListener);

	// Switch log messages to the log file only.
	caerLogFileDescriptorsSet(CAER_LOG_FILE_FD, -1);

	// Make sure log file gets flushed at exit time.
	atexit(&caerLogShutDownWriteBack);

	// Send any log messages out via SSHS from now on.
	caerLogCallbackSet(&caerLogMessagesToSSHS);

	// Now that config is initialized (has to be!) and logging too, we can
	// set the SSHS logger to use our internal logger too.
	sshsSetGlobalErrorLogCallback(&caerLogSSHSLogger);

	// Log sub-system initialized fully and correctly, log this.
	caerLog(CAER_LOG_NOTICE, "Logger", "Initialization successful with log-level %" PRIu8 ".", logLevel);
}

static void caerLogMessagesToSSHS(const char *msg, size_t msgLength) {
	sshsNode logNode = sshsGetNode(sshsGetGlobal(), "/caer/logger/");

	sshs_node_attr_value logMessage;
	logMessage.string = const_cast<char *>(msg);

	// Remove trailing newline (replace with NUL terminator).
	// HACK: this works by bypassing const on the input message.
	// We do know this is fine due to caerLog() putting msg in RW memory
	// and passing it to the callback last by design.
	logMessage.string[msgLength - 1] = '\0';

	sshsNodeUpdateReadOnlyAttribute(logNode, "lastLogMessage", SSHS_STRING, logMessage);
}

static void caerLogShutDownWriteBack(void) {
	caerLog(CAER_LOG_DEBUG, "Logger", "Shutting down ...");

	// Flush interactive outputs.
	fflush(stdout);
	fflush(stderr);

	// Ensure proper flushing and closing of the log file at shutdown.
	portable_fsync(CAER_LOG_FILE_FD);
	close(CAER_LOG_FILE_FD);
}

static void caerLogSSHSLogger(const char *msg, bool fatal) {
	if (fatal) {
		throw std::runtime_error(msg);
	}
	else {
		caerLog(CAER_LOG_ERROR, "SSHS", "%s", msg);
	}
}

static void caerLogLevelListener(sshsNode node, void *userData, enum sshs_node_attribute_events event,
	const char *changeKey, enum sshs_node_attr_value_type changeType, union sshs_node_attr_value changeValue) {
	UNUSED_ARGUMENT(node);
	UNUSED_ARGUMENT(userData);

	if (event == SSHS_ATTRIBUTE_MODIFIED && changeType == SSHS_INT && caerStrEquals(changeKey, "logLevel")) {
		// Update the global log level asynchronously.
		caerLogLevelSet((enum caer_log_level) changeValue.iint);
		caerLog(CAER_LOG_DEBUG, "Logger", "Log-level set to %" PRIi32 ".", changeValue.iint);
	}
}
