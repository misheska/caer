#include "davis_utils.h"

static void caerInputDAVISRPiConfigInit(dvConfigNode moduleNode);
static bool caerInputDAVISRPiInit(dvModuleData moduleData);
static void caerInputDAVISRPiExit(dvModuleData moduleData);

static const struct dvModuleFunctionsS DAVISRPiFunctions = {.moduleConfigInit = &caerInputDAVISRPiConfigInit,
	.moduleInit                                                                  = &caerInputDAVISRPiInit,
	.moduleRun                                                                   = &caerInputDAVISCommonRun,
	.moduleConfig                                                                = NULL,
	.moduleExit                                                                  = &caerInputDAVISRPiExit,
	.moduleReset                                                                 = NULL};

static const struct caer_event_stream_out DAVISRPiOutputs[]
	= {{.type = SPECIAL_EVENT}, {.type = POLARITY_EVENT}, {.type = FRAME_EVENT}, {.type = IMU6_EVENT}};

static const struct dvModuleInfoS DAVISRPiInfo = {
	.version           = 1,
	.name              = "DAVISRPi",
	.description       = "Connects to a DAVIS Raspberry-Pi camera module to get data.",
	.type              = DV_MODULE_INPUT,
	.memSize           = 0,
	.functions         = &DAVISRPiFunctions,
	.inputStreams      = NULL,
	.inputStreamsSize  = 0,
	.outputStreams     = DAVISRPiOutputs,
	.outputStreamsSize = CAER_EVENT_STREAM_OUT_SIZE(DAVISRPiOutputs),
};

dvModuleInfo dvModuleGetInfo(void) {
	return (&DAVISRPiInfo);
}

static void createDefaultAERConfiguration(dvModuleData moduleData, const char *nodePrefix);
static void sendDefaultConfiguration(dvModuleData moduleData, struct caer_davis_info *devInfo);

static void aerConfigSend(dvConfigNode node, dvModuleData moduleData);
static void aerConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

static void caerInputDAVISRPiConfigInit(dvConfigNode moduleNode) {
	caerInputDAVISCommonSystemConfigInit(moduleNode);
}

static bool caerInputDAVISRPiInit(dvModuleData moduleData) {
	dvModuleLog(moduleData, CAER_LOG_DEBUG, "Initializing module ...");

	// Start data acquisition, and correctly notify mainloop of new data and module of exceptional
	// shutdown cases (device pulled, ...).
	moduleData->moduleState = caerDeviceOpen(U16T(moduleData->moduleID), CAER_DEVICE_DAVIS_RPI, 0, 0, NULL);

	if (moduleData->moduleState == NULL) {
		// Failed to open device.
		return (false);
	}

	struct caer_davis_info devInfo = caerDavisInfoGet(moduleData->moduleState);

	caerInputDAVISCommonInit(moduleData, &devInfo);

	// Create default settings and send them to the device.
	createDefaultBiasConfiguration(moduleData, chipIDToName(devInfo.chipID, true), devInfo.chipID);
	createDefaultLogicConfiguration(moduleData, chipIDToName(devInfo.chipID, true), &devInfo);
	createDefaultAERConfiguration(moduleData, chipIDToName(devInfo.chipID, true));
	sendDefaultConfiguration(moduleData, &devInfo);

	// Start data acquisition.
	bool ret = caerDeviceDataStart(moduleData->moduleState, &caerMainloopDataNotifyIncrease,
		&caerMainloopDataNotifyDecrease, NULL, &moduleShutdownNotify, moduleData->moduleNode);

	if (!ret) {
		// Failed to start data acquisition, close device and exit.
		caerDeviceClose((caerDeviceHandle *) &moduleData->moduleState);

		return (false);
	}

	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, chipIDToName(devInfo.chipID, true));

	// Add config listeners last, to avoid having them dangling if Init doesn't succeed.
	dvConfigNode chipNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "chip/");
	dvConfigNodeAddAttributeListener(chipNode, moduleData, &chipConfigListener);

	dvConfigNode muxNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "multiplexer/");
	dvConfigNodeAddAttributeListener(muxNode, moduleData, &muxConfigListener);

	dvConfigNode dvsNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "dvs/");
	dvConfigNodeAddAttributeListener(dvsNode, moduleData, &dvsConfigListener);

	dvConfigNode apsNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "aps/");
	dvConfigNodeAddAttributeListener(apsNode, moduleData, &apsConfigListener);

	dvConfigNode imuNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "imu/");
	dvConfigNodeAddAttributeListener(imuNode, moduleData, &imuConfigListener);

	dvConfigNode extNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "externalInput/");
	dvConfigNodeAddAttributeListener(extNode, moduleData, &extInputConfigListener);

	dvConfigNode aerNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "aer/");
	dvConfigNodeAddAttributeListener(aerNode, moduleData, &aerConfigListener);

	dvConfigNode sysNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "system/");
	dvConfigNodeAddAttributeListener(sysNode, moduleData, &systemConfigListener);

	dvConfigNode biasNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "bias/");

	size_t biasNodesLength = 0;
	dvConfigNode *biasNodes    = dvConfigNodeGetChildren(biasNode, &biasNodesLength);

	if (biasNodes != NULL) {
		for (size_t i = 0; i < biasNodesLength; i++) {
			// Add listener for this particular bias.
			dvConfigNodeAddAttributeListener(biasNodes[i], moduleData, &biasConfigListener);
		}

		free(biasNodes);
	}

	dvConfigNodeAddAttributeListener(moduleData->moduleNode, moduleData, &logLevelListener);

	return (true);
}

static void caerInputDAVISRPiExit(dvModuleData moduleData) {
	// Device related configuration has its own sub-node.
	struct caer_davis_info devInfo = caerDavisInfoGet(moduleData->moduleState);
	dvConfigNode deviceConfigNode      = dvConfigNodeGetRelativeNode(moduleData->moduleNode, chipIDToName(devInfo.chipID, true));

	// Remove listener, which can reference invalid memory in userData.
	dvConfigNodeRemoveAttributeListener(moduleData->moduleNode, moduleData, &logLevelListener);

	dvConfigNode chipNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "chip/");
	dvConfigNodeRemoveAttributeListener(chipNode, moduleData, &chipConfigListener);

	dvConfigNode muxNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "multiplexer/");
	dvConfigNodeRemoveAttributeListener(muxNode, moduleData, &muxConfigListener);

	dvConfigNode dvsNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "dvs/");
	dvConfigNodeRemoveAttributeListener(dvsNode, moduleData, &dvsConfigListener);

	dvConfigNode apsNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "aps/");
	dvConfigNodeRemoveAttributeListener(apsNode, moduleData, &apsConfigListener);

	dvConfigNode imuNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "imu/");
	dvConfigNodeRemoveAttributeListener(imuNode, moduleData, &imuConfigListener);

	dvConfigNode extNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "externalInput/");
	dvConfigNodeRemoveAttributeListener(extNode, moduleData, &extInputConfigListener);

	dvConfigNode aerNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "aer/");
	dvConfigNodeRemoveAttributeListener(aerNode, moduleData, &aerConfigListener);

	dvConfigNode sysNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "system/");
	dvConfigNodeRemoveAttributeListener(sysNode, moduleData, &systemConfigListener);

	dvConfigNode biasNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "bias/");

	size_t biasNodesLength = 0;
	dvConfigNode *biasNodes    = dvConfigNodeGetChildren(biasNode, &biasNodesLength);

	if (biasNodes != NULL) {
		for (size_t i = 0; i < biasNodesLength; i++) {
			// Remove listener for this particular bias.
			dvConfigNodeRemoveAttributeListener(biasNodes[i], moduleData, &biasConfigListener);
		}

		free(biasNodes);
	}

	// Ensure Exposure value is coherent with libcaer.
	dvConfigNodeAttributeUpdaterRemoveAll(apsNode);
	dvConfigNodePutAttribute(
		apsNode, "Exposure", DVCFG_TYPE_INT, apsExposureUpdater(moduleData->moduleState, "Exposure", DVCFG_TYPE_INT));

	// Remove statistics updaters.
	dvConfigNode statNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "statistics/");
	dvConfigNodeAttributeUpdaterRemoveAll(statNode);

	caerDeviceDataStop(moduleData->moduleState);

	caerDeviceClose((caerDeviceHandle *) &moduleData->moduleState);

	// Clear sourceInfo node.
	dvConfigNode sourceInfoNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "sourceInfo/");
	dvConfigNodeRemoveAllAttributes(sourceInfoNode);
}

static void createDefaultAERConfiguration(dvModuleData moduleData, const char *nodePrefix) {
	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, nodePrefix);

	// Subsystem 9: DDR AER output configuration.
	dvConfigNode aerNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "aer/");
	dvConfigNodeCreateBool(aerNode, "Run", true, DVCFG_FLAGS_NORMAL,
		"Enable the DDR AER output state machine (FPGA to Raspberry-Pi data exchange).");
}

static void sendDefaultConfiguration(dvModuleData moduleData, struct caer_davis_info *devInfo) {
	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, chipIDToName(devInfo->chipID, true));

	// Send cAER configuration to libcaer and device.
	biasConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "bias/"), moduleData, devInfo);
	chipConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "chip/"), moduleData, devInfo);
	systemConfigSend(dvConfigNodeGetRelativeNode(moduleData->moduleNode, "system/"), moduleData);
	aerConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "aer/"), moduleData);
	muxConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "multiplexer/"), moduleData);
	dvsConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "dvs/"), moduleData, devInfo);
	apsConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "aps/"), moduleData, devInfo);
	imuConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "imu/"), moduleData, devInfo);
	extInputConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "externalInput/"), moduleData, devInfo);
}

static void aerConfigSend(dvConfigNode node, dvModuleData moduleData) {
	caerDeviceConfigSet(
		moduleData->moduleState, DAVIS_CONFIG_DDRAER, DAVIS_CONFIG_DDRAER_RUN, dvConfigNodeGetBool(node, "Run"));
}

static void aerConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	dvModuleData moduleData = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "Run")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DAVIS_CONFIG_DDRAER, DAVIS_CONFIG_DDRAER_RUN, changeValue.boolean);
		}
	}
}
