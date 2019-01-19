#include "davis_utils.h"

static void caerInputDAVISRPiConfigInit(dvConfigNode moduleNode);
static bool caerInputDAVISRPiInit(caerModuleData moduleData);
static void caerInputDAVISRPiExit(caerModuleData moduleData);

static const struct caer_module_functions DAVISRPiFunctions = {.moduleConfigInit = &caerInputDAVISRPiConfigInit,
	.moduleInit                                                                  = &caerInputDAVISRPiInit,
	.moduleRun                                                                   = &caerInputDAVISCommonRun,
	.moduleConfig                                                                = NULL,
	.moduleExit                                                                  = &caerInputDAVISRPiExit,
	.moduleReset                                                                 = NULL};

static const struct caer_event_stream_out DAVISRPiOutputs[]
	= {{.type = SPECIAL_EVENT}, {.type = POLARITY_EVENT}, {.type = FRAME_EVENT}, {.type = IMU6_EVENT}};

static const struct caer_module_info DAVISRPiInfo = {
	.version           = 1,
	.name              = "DAVISRPi",
	.description       = "Connects to a DAVIS Raspberry-Pi camera module to get data.",
	.type              = CAER_MODULE_INPUT,
	.memSize           = 0,
	.functions         = &DAVISRPiFunctions,
	.inputStreams      = NULL,
	.inputStreamsSize  = 0,
	.outputStreams     = DAVISRPiOutputs,
	.outputStreamsSize = CAER_EVENT_STREAM_OUT_SIZE(DAVISRPiOutputs),
};

caerModuleInfo caerModuleGetInfo(void) {
	return (&DAVISRPiInfo);
}

static void createDefaultAERConfiguration(caerModuleData moduleData, const char *nodePrefix);
static void sendDefaultConfiguration(caerModuleData moduleData, struct caer_davis_info *devInfo);

static void aerConfigSend(dvConfigNode node, caerModuleData moduleData);
static void aerConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

static void caerInputDAVISRPiConfigInit(dvConfigNode moduleNode) {
	caerInputDAVISCommonSystemConfigInit(moduleNode);
}

static bool caerInputDAVISRPiInit(caerModuleData moduleData) {
	caerModuleLog(moduleData, CAER_LOG_DEBUG, "Initializing module ...");

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
	dvConfigNode deviceConfigNode = sshsNodeGetRelativeNode(moduleData->moduleNode, chipIDToName(devInfo.chipID, true));

	// Add config listeners last, to avoid having them dangling if Init doesn't succeed.
	dvConfigNode chipNode = sshsNodeGetRelativeNode(deviceConfigNode, "chip/");
	sshsNodeAddAttributeListener(chipNode, moduleData, &chipConfigListener);

	dvConfigNode muxNode = sshsNodeGetRelativeNode(deviceConfigNode, "multiplexer/");
	sshsNodeAddAttributeListener(muxNode, moduleData, &muxConfigListener);

	dvConfigNode dvsNode = sshsNodeGetRelativeNode(deviceConfigNode, "dvs/");
	sshsNodeAddAttributeListener(dvsNode, moduleData, &dvsConfigListener);

	dvConfigNode apsNode = sshsNodeGetRelativeNode(deviceConfigNode, "aps/");
	sshsNodeAddAttributeListener(apsNode, moduleData, &apsConfigListener);

	dvConfigNode imuNode = sshsNodeGetRelativeNode(deviceConfigNode, "imu/");
	sshsNodeAddAttributeListener(imuNode, moduleData, &imuConfigListener);

	dvConfigNode extNode = sshsNodeGetRelativeNode(deviceConfigNode, "externalInput/");
	sshsNodeAddAttributeListener(extNode, moduleData, &extInputConfigListener);

	dvConfigNode aerNode = sshsNodeGetRelativeNode(deviceConfigNode, "aer/");
	sshsNodeAddAttributeListener(aerNode, moduleData, &aerConfigListener);

	dvConfigNode sysNode = sshsNodeGetRelativeNode(moduleData->moduleNode, "system/");
	sshsNodeAddAttributeListener(sysNode, moduleData, &systemConfigListener);

	dvConfigNode biasNode = sshsNodeGetRelativeNode(deviceConfigNode, "bias/");

	size_t biasNodesLength = 0;
	dvConfigNode *biasNodes    = sshsNodeGetChildren(biasNode, &biasNodesLength);

	if (biasNodes != NULL) {
		for (size_t i = 0; i < biasNodesLength; i++) {
			// Add listener for this particular bias.
			sshsNodeAddAttributeListener(biasNodes[i], moduleData, &biasConfigListener);
		}

		free(biasNodes);
	}

	sshsNodeAddAttributeListener(moduleData->moduleNode, moduleData, &logLevelListener);

	return (true);
}

static void caerInputDAVISRPiExit(caerModuleData moduleData) {
	// Device related configuration has its own sub-node.
	struct caer_davis_info devInfo = caerDavisInfoGet(moduleData->moduleState);
	dvConfigNode deviceConfigNode      = sshsNodeGetRelativeNode(moduleData->moduleNode, chipIDToName(devInfo.chipID, true));

	// Remove listener, which can reference invalid memory in userData.
	sshsNodeRemoveAttributeListener(moduleData->moduleNode, moduleData, &logLevelListener);

	dvConfigNode chipNode = sshsNodeGetRelativeNode(deviceConfigNode, "chip/");
	sshsNodeRemoveAttributeListener(chipNode, moduleData, &chipConfigListener);

	dvConfigNode muxNode = sshsNodeGetRelativeNode(deviceConfigNode, "multiplexer/");
	sshsNodeRemoveAttributeListener(muxNode, moduleData, &muxConfigListener);

	dvConfigNode dvsNode = sshsNodeGetRelativeNode(deviceConfigNode, "dvs/");
	sshsNodeRemoveAttributeListener(dvsNode, moduleData, &dvsConfigListener);

	dvConfigNode apsNode = sshsNodeGetRelativeNode(deviceConfigNode, "aps/");
	sshsNodeRemoveAttributeListener(apsNode, moduleData, &apsConfigListener);

	dvConfigNode imuNode = sshsNodeGetRelativeNode(deviceConfigNode, "imu/");
	sshsNodeRemoveAttributeListener(imuNode, moduleData, &imuConfigListener);

	dvConfigNode extNode = sshsNodeGetRelativeNode(deviceConfigNode, "externalInput/");
	sshsNodeRemoveAttributeListener(extNode, moduleData, &extInputConfigListener);

	dvConfigNode aerNode = sshsNodeGetRelativeNode(deviceConfigNode, "aer/");
	sshsNodeRemoveAttributeListener(aerNode, moduleData, &aerConfigListener);

	dvConfigNode sysNode = sshsNodeGetRelativeNode(moduleData->moduleNode, "system/");
	sshsNodeRemoveAttributeListener(sysNode, moduleData, &systemConfigListener);

	dvConfigNode biasNode = sshsNodeGetRelativeNode(deviceConfigNode, "bias/");

	size_t biasNodesLength = 0;
	dvConfigNode *biasNodes    = sshsNodeGetChildren(biasNode, &biasNodesLength);

	if (biasNodes != NULL) {
		for (size_t i = 0; i < biasNodesLength; i++) {
			// Remove listener for this particular bias.
			sshsNodeRemoveAttributeListener(biasNodes[i], moduleData, &biasConfigListener);
		}

		free(biasNodes);
	}

	// Ensure Exposure value is coherent with libcaer.
	dvConfigNodeAttributeUpdaterRemoveAll(apsNode);
	sshsNodePutAttribute(
		apsNode, "Exposure", DVCFG_TYPE_INT, apsExposureUpdater(moduleData->moduleState, "Exposure", DVCFG_TYPE_INT));

	// Remove statistics updaters.
	dvConfigNode statNode = sshsNodeGetRelativeNode(deviceConfigNode, "statistics/");
	dvConfigNodeAttributeUpdaterRemoveAll(statNode);

	caerDeviceDataStop(moduleData->moduleState);

	caerDeviceClose((caerDeviceHandle *) &moduleData->moduleState);

	// Clear sourceInfo node.
	dvConfigNode sourceInfoNode = sshsNodeGetRelativeNode(moduleData->moduleNode, "sourceInfo/");
	sshsNodeRemoveAllAttributes(sourceInfoNode);
}

static void createDefaultAERConfiguration(caerModuleData moduleData, const char *nodePrefix) {
	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode = sshsNodeGetRelativeNode(moduleData->moduleNode, nodePrefix);

	// Subsystem 9: DDR AER output configuration.
	dvConfigNode aerNode = sshsNodeGetRelativeNode(deviceConfigNode, "aer/");
	sshsNodeCreateBool(aerNode, "Run", true, DVCFG_FLAGS_NORMAL,
		"Enable the DDR AER output state machine (FPGA to Raspberry-Pi data exchange).");
}

static void sendDefaultConfiguration(caerModuleData moduleData, struct caer_davis_info *devInfo) {
	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode = sshsNodeGetRelativeNode(moduleData->moduleNode, chipIDToName(devInfo->chipID, true));

	// Send cAER configuration to libcaer and device.
	biasConfigSend(sshsNodeGetRelativeNode(deviceConfigNode, "bias/"), moduleData, devInfo);
	chipConfigSend(sshsNodeGetRelativeNode(deviceConfigNode, "chip/"), moduleData, devInfo);
	systemConfigSend(sshsNodeGetRelativeNode(moduleData->moduleNode, "system/"), moduleData);
	aerConfigSend(sshsNodeGetRelativeNode(deviceConfigNode, "aer/"), moduleData);
	muxConfigSend(sshsNodeGetRelativeNode(deviceConfigNode, "multiplexer/"), moduleData);
	dvsConfigSend(sshsNodeGetRelativeNode(deviceConfigNode, "dvs/"), moduleData, devInfo);
	apsConfigSend(sshsNodeGetRelativeNode(deviceConfigNode, "aps/"), moduleData, devInfo);
	imuConfigSend(sshsNodeGetRelativeNode(deviceConfigNode, "imu/"), moduleData, devInfo);
	extInputConfigSend(sshsNodeGetRelativeNode(deviceConfigNode, "externalInput/"), moduleData, devInfo);
}

static void aerConfigSend(dvConfigNode node, caerModuleData moduleData) {
	caerDeviceConfigSet(
		moduleData->moduleState, DAVIS_CONFIG_DDRAER, DAVIS_CONFIG_DDRAER_RUN, sshsNodeGetBool(node, "Run"));
}

static void aerConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	caerModuleData moduleData = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "Run")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DAVIS_CONFIG_DDRAER, DAVIS_CONFIG_DDRAER_RUN, changeValue.boolean);
		}
	}
}
