#include "dv-sdk/cross/portable_io.h"
#include "dv-sdk/cross/portable_time.h"
#include "dv-sdk/mainloop.h"

#include "output_common.h"

#include <fcntl.h>
#include <time.h>

#define DEFAULT_PREFIX "caerOut"
#define MAX_PREFIX_LENGTH 128

static bool caerOutputFileInit(dvModuleData moduleData);

static const struct dvModuleFunctionsS OutputFileFunctions = {.moduleInit = &caerOutputFileInit,
	.moduleRun                                                               = &caerOutputCommonRun,
	.moduleConfig                                                            = NULL,
	.moduleExit                                                              = &caerOutputCommonExit,
	.moduleReset                                                             = &caerOutputCommonReset};

static const struct caer_event_stream_in OutputFileInputs[] = {{.type = -1, .number = -1, .readOnly = true}};

static const struct dvModuleInfoS OutputFileInfo = {
	.version           = 1,
	.name              = "FileOutput",
	.description       = "Write AEDAT 3 data out to a file.",
	.type              = DV_MODULE_OUTPUT,
	.memSize           = sizeof(struct output_common_state),
	.functions         = &OutputFileFunctions,
	.inputStreams      = OutputFileInputs,
	.inputStreamsSize  = CAER_EVENT_STREAM_IN_SIZE(OutputFileInputs),
	.outputStreams     = NULL,
	.outputStreamsSize = 0,
};

dvModuleInfo dvModuleGetInfo(void) {
	return (&OutputFileInfo);
}

static char *getUserHomeDirectory(dvModuleData moduleData);
static char *getFullFilePath(dvModuleData moduleData, const char *directory, const char *prefix);

// Remember to free strings returned by this.
static char *getUserHomeDirectory(dvModuleData moduleData) {
	size_t homeDirLength = PATH_MAX;

	// Allocate memory for home directory path.
	char *homeDir = malloc(homeDirLength);
	if (homeDir == NULL) {
		dvModuleLog(moduleData, CAER_LOG_ERROR, "Failed to allocate memory for home directory string.");
		return (NULL);
	}

	// Discover home directory path, use libuv for cross-platform support.
	int retVal = uv_os_homedir(homeDir, &homeDirLength);
	if (retVal < 0) {
		dvModuleLog(moduleData, CAER_LOG_ERROR, "uv_os_homedir failed, error %d (%s).", retVal, uv_err_name(retVal));
		return (NULL);
	}

	return (homeDir);
}

static char *getFullFilePath(dvModuleData moduleData, const char *directory, const char *prefix) {
	// First get time suffix string.
	struct tm currentTimeStruct = portable_clock_localtime();

	// Following time format uses exactly 19 characters (5 separators,
	// 4 year, 2 month, 2 day, 2 hours, 2 minutes, 2 seconds).
	size_t currentTimeStringLength = 19;
	char currentTimeString[currentTimeStringLength + 1]; // + 1 for terminating NUL byte.
	strftime(currentTimeString, currentTimeStringLength + 1, "%Y_%m_%d_%H_%M_%S", &currentTimeStruct);

	if (caerStrEquals(prefix, "")) {
		// If the prefix is the empty string, use a minimal one.
		prefix = DEFAULT_PREFIX;
	}

	// Assemble together: directory/prefix-time.aedat
	size_t filePathLength = strlen(directory) + strlen(prefix) + currentTimeStringLength + 9;
	// 1 for the directory/prefix separating slash, 1 for prefix-time separating
	// dash, 6 for file extension, 1 for terminating NUL byte = +9.

	char *filePath = malloc(filePathLength);
	if (filePath == NULL) {
		dvModuleLog(moduleData, CAER_LOG_CRITICAL, "Unable to allocate memory for full file path.");
		return (NULL);
	}

	snprintf(filePath, filePathLength, "%s/%s-%s.aedat", directory, prefix, currentTimeString);

	return (filePath);
}

static bool caerOutputFileInit(dvModuleData moduleData) {
	// First, always create all needed setting nodes, set their default values
	// and add their listeners.
	char *userHomeDir = getUserHomeDirectory(moduleData);
	if (userHomeDir == NULL) {
		// dvModuleLog() called inside getUserHomeDirectory().
		return (false);
	}

	dvConfigNodeCreateString(moduleData->moduleNode, "directory", userHomeDir, 1, (PATH_MAX - MAX_PREFIX_LENGTH),
		DVCFG_FLAGS_NORMAL, "Directory to write output data files in.");
	free(userHomeDir);

	// Support file-chooser in GUI, select any directory.
	dvConfigNodeAttributeModifierFileChooser(moduleData->moduleNode, "directory", "DIRECTORY");

	dvConfigNodeCreateString(moduleData->moduleNode, "prefix", DEFAULT_PREFIX, 1, MAX_PREFIX_LENGTH, DVCFG_FLAGS_NORMAL,
		"Output data files name prefix.");

	// Generate current file name and open it.
	char *directory = dvConfigNodeGetString(moduleData->moduleNode, "directory");
	char *prefix    = dvConfigNodeGetString(moduleData->moduleNode, "prefix");

	char *filePath = getFullFilePath(moduleData, directory, prefix);
	free(directory);
	free(prefix);

	if (filePath == NULL) {
		// dvModuleLog() called inside getFullFilePath().
		return (false);
	}

	int fileFd = open(filePath, O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP);
	if (fileFd < 0) {
		dvModuleLog(moduleData, CAER_LOG_CRITICAL,
			"Could not create or open output file '%s' for writing. Error: %d.", filePath, errno);
		free(filePath);

		return (false);
	}

	dvModuleLog(moduleData, CAER_LOG_INFO, "Opened output file '%s' successfully for writing.", filePath);
	free(filePath);

	if (!caerOutputCommonInit(moduleData, fileFd, NULL)) {
		close(fileFd);

		return (false);
	}

	return (true);
}
