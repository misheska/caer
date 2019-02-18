#include "dv-sdk/cross/portable_io.h"
#include "dv-sdk/mainloop.h"

#include "input_common.h"

#include <fcntl.h>
#include <sys/types.h>

static bool caerInputFileInit(dvModuleData moduleData);

static const struct dvModuleFunctionsS InputFileFunctions = {
	.moduleInit   = &caerInputFileInit,
	.moduleRun    = &caerInputCommonRun,
	.moduleConfig = NULL,
	.moduleExit   = &caerInputCommonExit,
};

static const struct caer_event_stream_out InputFileOutputs[] = {{.type = -1}};

static const struct dvModuleInfoS InputFileInfo = {
	.version           = 1,
	.description       = "Read AEDAT data from a file.",
	.type              = DV_MODULE_INPUT,
	.memSize           = sizeof(struct input_common_state),
	.functions         = &InputFileFunctions,
	.inputStreams      = NULL,
	.inputStreamsSize  = 0,
	.outputStreams     = InputFileOutputs,
	.outputStreamsSize = CAER_EVENT_STREAM_OUT_SIZE(InputFileOutputs),
};

dvModuleInfo dvModuleGetInfo(void) {
	return (&InputFileInfo);
}

static bool caerInputFileInit(dvModuleData moduleData) {
	dvConfigNodeCreateString(
		moduleData->moduleNode, "filePath", "", 0, PATH_MAX, DVCFG_FLAGS_NORMAL, "File path for reading input data.");
	dvConfigNodeAttributeModifierFileChooser(moduleData->moduleNode, "filePath", "LOAD:aedat");

	char *filePath = dvConfigNodeGetString(moduleData->moduleNode, "filePath");

	if (caerStrEquals(filePath, "")) {
		free(filePath);

		dvModuleLog(moduleData, CAER_LOG_ERROR, "No input file given, please specify the 'filePath' parameter.");
		return (false);
	}

	int fileFd = open(filePath, O_RDONLY);
	if (fileFd < 0) {
		dvModuleLog(
			moduleData, CAER_LOG_CRITICAL, "Could not open input file '%s' for reading. Error: %d.", filePath, errno);
		free(filePath);

		return (false);
	}

	dvModuleLog(moduleData, CAER_LOG_INFO, "Opened input file '%s' successfully for reading.", filePath);
	free(filePath);

	if (!caerInputCommonInit(moduleData, fileFd, false, false)) {
		close(fileFd);

		return (false);
	}

	return (true);
}
