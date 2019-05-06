#include "davis_utils.h"

#include <time.h>

static void caerInputDAVISStaticInit(dvModuleData moduleData);
static bool caerInputDAVISInit(dvModuleData moduleData);
static void caerInputDAVISExit(dvModuleData moduleData);

static const struct dvModuleFunctionsS DAVISFunctions = {
	.moduleStaticInit = &caerInputDAVISStaticInit,
	.moduleInit       = &caerInputDAVISInit,
	.moduleRun        = &caerInputDAVISCommonRun,
	.moduleConfig     = NULL,
	.moduleExit       = &caerInputDAVISExit,
};

static const struct dvModuleInfoS DAVISInfo = {
	.version     = 1,
	.description = "Connects to a DAVIS camera to get data.",
	.memSize     = 0,
	.functions   = &DAVISFunctions,
};

dvModuleInfo dvModuleGetInfo(void) {
	return (&DAVISInfo);
}

static void createDefaultUSBConfiguration(dvModuleData moduleData, const char *nodePrefix);
static void sendDefaultConfiguration(dvModuleData moduleData, struct caer_davis_info *devInfo);

static void usbConfigSend(dvConfigNode node, dvModuleData moduleData);
static void usbConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

static void caerInputDAVISStaticInit(dvModuleData moduleData) {
	dvConfigNode moduleNode = moduleData->moduleNode;

	// USB port/bus/SN settings/restrictions.
	// These can be used to force connection to one specific device at startup.
	dvConfigNodeCreateInt(moduleNode, "busNumber", 0, 0, INT16_MAX, DVCFG_FLAGS_NORMAL, "USB bus number restriction.");
	dvConfigNodeCreateInt(
		moduleNode, "devAddress", 0, 0, INT16_MAX, DVCFG_FLAGS_NORMAL, "USB device address restriction.");
	dvConfigNodeCreateString(
		moduleNode, "serialNumber", "", 0, 8, DVCFG_FLAGS_NORMAL, "USB serial number restriction.");

	caerInputDAVISCommonSystemConfigInit(moduleData);
}

static bool caerInputDAVISInit(dvModuleData moduleData) {
	dvLog(CAER_LOG_DEBUG, "Initializing module ...");

	// Start data acquisition, and correctly notify mainloop of new data and module of exceptional
	// shutdown cases (device pulled, ...).
	char *serialNumber = dvConfigNodeGetString(moduleData->moduleNode, "serialNumber");
	moduleData->moduleState
		= caerDeviceOpen(0, CAER_DEVICE_DAVIS, U8T(dvConfigNodeGetInt(moduleData->moduleNode, "busNumber")),
			U8T(dvConfigNodeGetInt(moduleData->moduleNode, "devAddress")), serialNumber);
	free(serialNumber);

	if (moduleData->moduleState == NULL) {
		// Failed to open device.
		return (false);
	}

	struct caer_davis_info devInfo = caerDavisInfoGet(moduleData->moduleState);

	caerInputDAVISCommonInit(moduleData, &devInfo);

	// Create default settings and send them to the device.
	createDefaultBiasConfiguration(moduleData, chipIDToName(devInfo.chipID, true), devInfo.chipID);
	createDefaultLogicConfiguration(moduleData, chipIDToName(devInfo.chipID, true), &devInfo);
	createDefaultUSBConfiguration(moduleData, chipIDToName(devInfo.chipID, true));

	// Start data acquisition.
	bool ret
		= caerDeviceDataStart(moduleData->moduleState, NULL, NULL, NULL, &moduleShutdownNotify, moduleData->moduleNode);

	if (!ret) {
		// Failed to start data acquisition, close device and exit.
		caerDeviceClose((caerDeviceHandle *) &moduleData->moduleState);

		return (false);
	}

	// Send configuration, enabling data capture as requested.
	sendDefaultConfiguration(moduleData, &devInfo);

	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode
		= dvConfigNodeGetRelativeNode(moduleData->moduleNode, chipIDToName(devInfo.chipID, true));

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

	dvConfigNode usbNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "usb/");
	dvConfigNodeAddAttributeListener(usbNode, moduleData, &usbConfigListener);

	dvConfigNode sysNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "system/");
	dvConfigNodeAddAttributeListener(sysNode, moduleData, &systemConfigListener);

	dvConfigNode biasNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "bias/");

	size_t biasNodesLength  = 0;
	dvConfigNode *biasNodes = dvConfigNodeGetChildren(biasNode, &biasNodesLength);

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

static void caerInputDAVISExit(dvModuleData moduleData) {
	// Device related configuration has its own sub-node.
	struct caer_davis_info devInfo = caerDavisInfoGet(moduleData->moduleState);
	dvConfigNode deviceConfigNode
		= dvConfigNodeGetRelativeNode(moduleData->moduleNode, chipIDToName(devInfo.chipID, true));

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

	dvConfigNode usbNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "usb/");
	dvConfigNodeRemoveAttributeListener(usbNode, moduleData, &usbConfigListener);

	dvConfigNode sysNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "system/");
	dvConfigNodeRemoveAttributeListener(sysNode, moduleData, &systemConfigListener);

	dvConfigNode biasNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "bias/");

	size_t biasNodesLength  = 0;
	dvConfigNode *biasNodes = dvConfigNodeGetChildren(biasNode, &biasNodesLength);

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
	if (dvConfigNodeExistsRelativeNode(deviceConfigNode, "statistics/")) {
		dvConfigNode statNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "statistics/");
		dvConfigNodeAttributeUpdaterRemoveAll(statNode);
	}

	caerDeviceDataStop(moduleData->moduleState);

	caerDeviceClose((caerDeviceHandle *) &moduleData->moduleState);

	// Clear sourceInfo node.
	dvConfigNode sourceInfoNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "sourceInfo/");
	dvConfigNodeRemoveAllAttributes(sourceInfoNode);
}

static void createDefaultUSBConfiguration(dvModuleData moduleData, const char *nodePrefix) {
	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, nodePrefix);

	// Subsystem 9: FX2/3 USB Configuration and USB buffer settings.
	dvConfigNode usbNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "usb/");

	dvConfigNodeAttributeModifierPriorityAttributes(usbNode, "");

	dvConfigNodeCreateBool(
		usbNode, "Run", true, DVCFG_FLAGS_NORMAL, "Enable the USB state machine (FPGA to USB data exchange).");
	dvConfigNodeCreateInt(usbNode, "EarlyPacketDelay", 8, 1, 8000, DVCFG_FLAGS_NORMAL,
		"Send early USB packets if this timeout is reached (in 125µs time-slices).");

	dvConfigNodeCreateInt(usbNode, "BufferNumber", 8, 2, 128, DVCFG_FLAGS_NORMAL, "Number of USB transfers.");
	dvConfigNodeCreateInt(usbNode, "BufferSize", 8192, 512, 32768, DVCFG_FLAGS_NORMAL,
		"Size in bytes of data buffers for USB transfers.");
}

static void sendDefaultConfiguration(dvModuleData moduleData, struct caer_davis_info *devInfo) {
	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode
		= dvConfigNodeGetRelativeNode(moduleData->moduleNode, chipIDToName(devInfo->chipID, true));

	dvConfigNode muxNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "multiplexer/");

	// Send cAER configuration to libcaer and device.
	biasConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "bias/"), moduleData, devInfo);
	chipConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "chip/"), moduleData, devInfo);
	caerDeviceConfigSet(
		moduleData->moduleState, DAVIS_CONFIG_MUX, DAVIS_CONFIG_MUX_RUN_CHIP, dvConfigNodeGetBool(muxNode, "RunChip"));

	// Wait 200 ms for biases to stabilize.
	struct timespec biasEnSleep = {.tv_sec = 0, .tv_nsec = 200000000};
	nanosleep(&biasEnSleep, NULL);

	systemConfigSend(dvConfigNodeGetRelativeNode(moduleData->moduleNode, "system/"), moduleData);
	usbConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "usb/"), moduleData);
	muxConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "multiplexer/"), moduleData);

	// Wait 50 ms for data transfer to be ready.
	struct timespec noDataSleep = {.tv_sec = 0, .tv_nsec = 50000000};
	nanosleep(&noDataSleep, NULL);

	dvsConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "dvs/"), moduleData, devInfo);
	apsConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "aps/"), moduleData, devInfo);
	imuConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "imu/"), moduleData, devInfo);
	extInputConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "externalInput/"), moduleData, devInfo);
}

static void usbConfigSend(dvConfigNode node, dvModuleData moduleData) {
	caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_USB, CAER_HOST_CONFIG_USB_BUFFER_NUMBER,
		U32T(dvConfigNodeGetInt(node, "BufferNumber")));
	caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_USB, CAER_HOST_CONFIG_USB_BUFFER_SIZE,
		U32T(dvConfigNodeGetInt(node, "BufferSize")));

	caerDeviceConfigSet(moduleData->moduleState, DAVIS_CONFIG_USB, DAVIS_CONFIG_USB_EARLY_PACKET_DELAY,
		U32T(dvConfigNodeGetInt(node, "EarlyPacketDelay")));
	caerDeviceConfigSet(
		moduleData->moduleState, DAVIS_CONFIG_USB, DAVIS_CONFIG_USB_RUN, dvConfigNodeGetBool(node, "Run"));
}

static void usbConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	dvModuleData moduleData = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "BufferNumber")) {
			caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_USB, CAER_HOST_CONFIG_USB_BUFFER_NUMBER,
				U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "BufferSize")) {
			caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_USB, CAER_HOST_CONFIG_USB_BUFFER_SIZE,
				U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "EarlyPacketDelay")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DAVIS_CONFIG_USB, DAVIS_CONFIG_USB_EARLY_PACKET_DELAY, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "Run")) {
			caerDeviceConfigSet(moduleData->moduleState, DAVIS_CONFIG_USB, DAVIS_CONFIG_USB_RUN, changeValue.boolean);
		}
	}
}
