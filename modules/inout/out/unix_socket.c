#include "dv-sdk/cross/portable_io.h"
#include "dv-sdk/mainloop.h"

#include "output_common.h"

static bool caerOutputUnixSocketInit(dvModuleData moduleData);

static const struct dvModuleFunctionsS OutputUnixSocketFunctions = {
	.moduleInit   = &caerOutputUnixSocketInit,
	.moduleRun    = &caerOutputCommonRun,
	.moduleConfig = NULL,
	.moduleExit   = &caerOutputCommonExit,
};

static const struct caer_event_stream_in OutputUnixSocketInputs[] = {{.type = -1, .number = -1, .readOnly = true}};

static const struct dvModuleInfoS OutputUnixSocketInfo = {
	.version           = 1,
	.description       = "Send AEDAT 3 data out to a Unix Socket (client mode).",
	.type              = DV_MODULE_OUTPUT,
	.memSize           = sizeof(struct output_common_state),
	.functions         = &OutputUnixSocketFunctions,
	.inputStreams      = OutputUnixSocketInputs,
	.inputStreamsSize  = CAER_EVENT_STREAM_IN_SIZE(OutputUnixSocketInputs),
	.outputStreams     = NULL,
	.outputStreamsSize = 0,
};

dvModuleInfo dvModuleGetInfo(void) {
	return (&OutputUnixSocketInfo);
}

static bool caerOutputUnixSocketInit(dvModuleData moduleData) {
	// First, always create all needed setting nodes, set their default values
	// and add their listeners.
	dvConfigNodeCreateString(moduleData->moduleNode, "socketPath", "/tmp/caer.sock", 2, PATH_MAX, DVCFG_FLAGS_NORMAL,
		"Unix Socket path for writing output data (client mode, connect to existing socket).");

	// Allocate memory.
	size_t numClients         = 1;
	outputCommonNetIO streams = malloc(sizeof(*streams) + (numClients * sizeof(uv_stream_t *)));
	if (streams == NULL) {
		dvModuleLog(moduleData, CAER_LOG_ERROR, "Failed to allocate memory for streams structure.");
		return (false);
	}

	uv_pipe_t *pipe = malloc(sizeof(uv_pipe_t));
	if (pipe == NULL) {
		free(streams);

		dvModuleLog(moduleData, CAER_LOG_ERROR, "Failed to allocate memory for network structure.");
		return (false);
	}

	uv_connect_t *connectRequest = malloc(sizeof(uv_connect_t));
	if (connectRequest == NULL) {
		free(pipe);
		free(streams);

		dvModuleLog(moduleData, CAER_LOG_ERROR, "Failed to allocate memory for network connection.");
		return (false);
	}

	// Initialize common info.
	streams->isTCP         = false;
	streams->isUDP         = false;
	streams->isPipe        = true;
	streams->activeClients = 0;
	streams->clientsSize   = numClients;
	streams->clients[0]    = NULL;
	streams->server        = NULL;

	// Remember address.
	streams->address = dvConfigNodeGetString(moduleData->moduleNode, "socketPath");

	pipe->data = streams;

	// Initialize loop and network handles.
	int retVal = uv_loop_init(&streams->loop);
	UV_RET_CHECK(retVal, moduleData->moduleSubSystemString, "uv_loop_init", free(connectRequest); free(pipe);
				 free(streams->address); free(streams); return (false));

	retVal = uv_pipe_init(&streams->loop, pipe, false);
	UV_RET_CHECK(retVal, moduleData->moduleSubSystemString, "uv_pipe_init", uv_loop_close(&streams->loop);
				 free(connectRequest); free(pipe); free(streams->address); free(streams); return (false));

	uv_pipe_connect(connectRequest, pipe, streams->address, &caerOutputCommonOnClientConnection);
	// No return value to check here.

	// Start.
	if (!caerOutputCommonInit(moduleData, -1, streams)) {
		libuvCloseLoopHandles(&streams->loop);
		uv_loop_close(&streams->loop);
		free(streams->address);
		free(streams);

		return (false);
	}

	return (true);
}
