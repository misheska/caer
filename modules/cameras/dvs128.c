#include <libcaer/events/packetContainer.h>
#include <libcaer/events/polarity.h>
#include <libcaer/events/special.h>

#include <libcaer/devices/dvs128.h>

#include "dv-sdk/mainloop.h"

static void caerInputDVS128ConfigInit(dvConfigNode moduleNode);
static bool caerInputDVS128Init(dvModuleData moduleData);
static void caerInputDVS128Run(dvModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out);
// CONFIG: Nothing to do here in the main thread!
// All configuration is asynchronous through config listeners.
static void caerInputDVS128Exit(dvModuleData moduleData);

static const struct dvModuleFunctionsS DVS128Functions = {
	.moduleConfigInit = &caerInputDVS128ConfigInit,
	.moduleInit       = &caerInputDVS128Init,
	.moduleRun        = &caerInputDVS128Run,
	.moduleConfig     = NULL,
	.moduleExit       = &caerInputDVS128Exit,
};

static const struct caer_event_stream_out DVS128Outputs[] = {{.type = SPECIAL_EVENT}, {.type = POLARITY_EVENT}};

static const struct dvModuleInfoS DVS128Info = {
	.version           = 1,
	.description       = "Connects to a DVS128 camera to get data.",
	.type              = DV_MODULE_INPUT,
	.memSize           = 0,
	.functions         = &DVS128Functions,
	.inputStreams      = NULL,
	.inputStreamsSize  = 0,
	.outputStreams     = DVS128Outputs,
	.outputStreamsSize = CAER_EVENT_STREAM_OUT_SIZE(DVS128Outputs),
};

dvModuleInfo dvModuleGetInfo(void) {
	return (&DVS128Info);
}

static void sendDefaultConfiguration(dvModuleData moduleData);
static void moduleShutdownNotify(void *p);
static void biasConfigSend(dvConfigNode node, dvModuleData moduleData);
static void biasConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void dvsConfigSend(dvConfigNode node, dvModuleData moduleData);
static void dvsConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void usbConfigSend(dvConfigNode node, dvModuleData moduleData);
static void usbConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void systemConfigSend(dvConfigNode node, dvModuleData moduleData);
static void systemConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void logLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

static void caerInputDVS128ConfigInit(dvConfigNode moduleNode) {
	// USB port/bus/SN settings/restrictions.
	// These can be used to force connection to one specific device at startup.
	dvConfigNodeCreateInt(moduleNode, "busNumber", 0, 0, INT16_MAX, DVCFG_FLAGS_NORMAL, "USB bus number restriction.");
	dvConfigNodeCreateInt(
		moduleNode, "devAddress", 0, 0, INT16_MAX, DVCFG_FLAGS_NORMAL, "USB device address restriction.");
	dvConfigNodeCreateString(
		moduleNode, "serialNumber", "", 0, 8, DVCFG_FLAGS_NORMAL, "USB serial number restriction.");

	// Add auto-restart setting.
	dvConfigNodeCreateBool(
		moduleNode, "autoRestart", true, DVCFG_FLAGS_NORMAL, "Automatically restart module after shutdown.");

	// Set default biases, from DVS128Fast.xml settings.
	dvConfigNode biasNode = dvConfigNodeGetRelativeNode(moduleNode, "bias/");

	dvConfigNodeAttributeModifierPriorityAttributes(biasNode, "diff,diffOn,diffOff");

	dvConfigNodeCreateInt(biasNode, "cas", 1992, 0, (0x01 << 24) - 1, DVCFG_FLAGS_NORMAL, "Photoreceptor cascode.");
	dvConfigNodeCreateInt(
		biasNode, "injGnd", 1108364, 0, (0x01 << 24) - 1, DVCFG_FLAGS_NORMAL, "Differentiator switch level.");
	dvConfigNodeCreateInt(
		biasNode, "reqPd", 16777215, 0, (0x01 << 24) - 1, DVCFG_FLAGS_NORMAL, "AER request pull-down.");
	dvConfigNodeCreateInt(
		biasNode, "puX", 8159221, 0, (0x01 << 24) - 1, DVCFG_FLAGS_NORMAL, "2nd dimension AER static pull-up.");
	dvConfigNodeCreateInt(
		biasNode, "diffOff", 132, 0, (0x01 << 24) - 1, DVCFG_FLAGS_NORMAL, "OFF threshold - lower to raise threshold.");
	dvConfigNodeCreateInt(
		biasNode, "req", 309590, 0, (0x01 << 24) - 1, DVCFG_FLAGS_NORMAL, "OFF request inverter bias.");
	dvConfigNodeCreateInt(biasNode, "refr", 969, 0, (0x01 << 24) - 1, DVCFG_FLAGS_NORMAL, "Refractory period.");
	dvConfigNodeCreateInt(
		biasNode, "puY", 16777215, 0, (0x01 << 24) - 1, DVCFG_FLAGS_NORMAL, "1st dimension AER static pull-up.");
	dvConfigNodeCreateInt(biasNode, "diffOn", 209996, 0, (0x01 << 24) - 1, DVCFG_FLAGS_NORMAL,
		"ON threshold - higher to raise threshold.");
	dvConfigNodeCreateInt(biasNode, "diff", 13125, 0, (0x01 << 24) - 1, DVCFG_FLAGS_NORMAL, "Differentiator.");
	dvConfigNodeCreateInt(biasNode, "foll", 271, 0, (0x01 << 24) - 1, DVCFG_FLAGS_NORMAL,
		"Source follower buffer between photoreceptor and differentiator.");
	dvConfigNodeCreateInt(biasNode, "pr", 217, 0, (0x01 << 24) - 1, DVCFG_FLAGS_NORMAL, "Photoreceptor.");

	// DVS settings.
	dvConfigNode dvsNode = dvConfigNodeGetRelativeNode(moduleNode, "dvs/");

	dvConfigNodeAttributeModifierPriorityAttributes(dvsNode, "Run,TimestampReset");

	dvConfigNodeCreateBool(dvsNode, "Run", true, DVCFG_FLAGS_NORMAL, "Run DVS to get polarity events.");
	dvConfigNodeCreateBool(dvsNode, "TimestampReset", false, DVCFG_FLAGS_NORMAL, "Reset timestamps to zero.");
	dvConfigNodeAttributeModifierButton(dvsNode, "TimestampReset", "EXECUTE");
	dvConfigNodeCreateBool(dvsNode, "ArrayReset", false, DVCFG_FLAGS_NORMAL, "Reset DVS pixel array.");
	dvConfigNodeAttributeModifierButton(dvsNode, "ArrayReset", "EXECUTE");

	// USB buffer settings.
	dvConfigNode usbNode = dvConfigNodeGetRelativeNode(moduleNode, "usb/");

	dvConfigNodeAttributeModifierPriorityAttributes(usbNode, "");

	dvConfigNodeCreateInt(usbNode, "BufferNumber", 8, 2, 128, DVCFG_FLAGS_NORMAL, "Number of USB transfers.");
	dvConfigNodeCreateInt(usbNode, "BufferSize", 4096, 512, 32768, DVCFG_FLAGS_NORMAL,
		"Size in bytes of data buffers for USB transfers.");

	dvConfigNode sysNode = dvConfigNodeGetRelativeNode(moduleNode, "system/");

	dvConfigNodeAttributeModifierPriorityAttributes(sysNode, "PacketContainerInterval");

	// Packet settings (size (in events) and time interval (in µs)).
	dvConfigNodeCreateInt(sysNode, "PacketContainerMaxPacketSize", 0, 0, 10 * 1024 * 1024, DVCFG_FLAGS_NORMAL,
		"Maximum packet size in events, when any packet reaches this size, the EventPacketContainer is sent for "
		"processing.");
	dvConfigNodeCreateInt(sysNode, "PacketContainerInterval", 10000, 1, 120 * 1000 * 1000, DVCFG_FLAGS_NORMAL,
		"Time interval in µs, each sent EventPacketContainer will span this interval.");

	// Ring-buffer setting (only changes value on module init/shutdown cycles).
	dvConfigNodeCreateInt(sysNode, "DataExchangeBufferSize", 64, 8, 1024, DVCFG_FLAGS_NORMAL,
		"Size of EventPacketContainer queue, used for transfers between data acquisition thread and mainloop.");
}

static bool caerInputDVS128Init(dvModuleData moduleData) {
	dvModuleLog(moduleData, CAER_LOG_DEBUG, "Initializing module ...");

	// Start data acquisition, and correctly notify mainloop of new data and module of exceptional
	// shutdown cases (device pulled, ...).
	char *serialNumber      = dvConfigNodeGetString(moduleData->moduleNode, "serialNumber");
	moduleData->moduleState = caerDeviceOpen(U16T(moduleData->moduleID), CAER_DEVICE_DVS128,
		U8T(dvConfigNodeGetInt(moduleData->moduleNode, "busNumber")),
		U8T(dvConfigNodeGetInt(moduleData->moduleNode, "devAddress")), serialNumber);
	free(serialNumber);

	if (moduleData->moduleState == NULL) {
		// Failed to open device.
		return (false);
	}

	// Initialize per-device log-level to module log-level.
	caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_LOG, CAER_HOST_CONFIG_LOG_LEVEL,
		atomic_load(&moduleData->moduleLogLevel));

	// Put global source information into config.
	struct caer_dvs128_info devInfo = caerDVS128InfoGet(moduleData->moduleState);

	dvConfigNode sourceInfoNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "sourceInfo/");

	dvConfigNodeCreateInt(sourceInfoNode, "logicVersion", devInfo.logicVersion, devInfo.logicVersion,
		devInfo.logicVersion, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Device FPGA logic version.");
	dvConfigNodeCreateBool(sourceInfoNode, "deviceIsMaster", devInfo.deviceIsMaster,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Timestamp synchronization support: device master status.");

	dvConfigNodeCreateInt(sourceInfoNode, "polaritySizeX", devInfo.dvsSizeX, devInfo.dvsSizeX, devInfo.dvsSizeX,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Polarity events width.");
	dvConfigNodeCreateInt(sourceInfoNode, "polaritySizeY", devInfo.dvsSizeY, devInfo.dvsSizeY, devInfo.dvsSizeY,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Polarity events height.");

	// Put source information for generic visualization, to be used to display and debug filter information.
	dvConfigNodeCreateInt(sourceInfoNode, "dataSizeX", devInfo.dvsSizeX, devInfo.dvsSizeX, devInfo.dvsSizeX,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Data width.");
	dvConfigNodeCreateInt(sourceInfoNode, "dataSizeY", devInfo.dvsSizeY, devInfo.dvsSizeY, devInfo.dvsSizeY,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Data height.");

	// Generate source string for output modules.
	size_t sourceStringLength = (size_t) snprintf(NULL, 0, "#Source %" PRIu16 ": DVS128\r\n", moduleData->moduleID);

	char sourceString[sourceStringLength + 1];
	snprintf(sourceString, sourceStringLength + 1, "#Source %" PRIu16 ": DVS128\r\n", moduleData->moduleID);
	sourceString[sourceStringLength] = '\0';

	dvConfigNodeCreateString(sourceInfoNode, "sourceString", sourceString, sourceStringLength, sourceStringLength,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Device source information.");

	// Generate sub-system string for module.
	char *prevAdditionStart = strchr(moduleData->moduleSubSystemString, '[');

	if (prevAdditionStart != NULL) {
		*prevAdditionStart = '\0';
	}

	size_t subSystemStringLength
		= (size_t) snprintf(NULL, 0, "%s[SN %s, %" PRIu8 ":%" PRIu8 "]", moduleData->moduleSubSystemString,
			devInfo.deviceSerialNumber, devInfo.deviceUSBBusNumber, devInfo.deviceUSBDeviceAddress);

	char subSystemString[subSystemStringLength + 1];
	snprintf(subSystemString, subSystemStringLength + 1, "%s[SN %s, %" PRIu8 ":%" PRIu8 "]",
		moduleData->moduleSubSystemString, devInfo.deviceSerialNumber, devInfo.deviceUSBBusNumber,
		devInfo.deviceUSBDeviceAddress);
	subSystemString[subSystemStringLength] = '\0';

	dvModuleSetLogString(moduleData, subSystemString);

	// Ensure good defaults for data acquisition settings.
	// No blocking behavior due to mainloop notification, and no auto-start of
	// all producers to ensure cAER settings are respected.
	caerDeviceConfigSet(
		moduleData->moduleState, CAER_HOST_CONFIG_DATAEXCHANGE, CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING, false);
	caerDeviceConfigSet(
		moduleData->moduleState, CAER_HOST_CONFIG_DATAEXCHANGE, CAER_HOST_CONFIG_DATAEXCHANGE_START_PRODUCERS, false);
	caerDeviceConfigSet(
		moduleData->moduleState, CAER_HOST_CONFIG_DATAEXCHANGE, CAER_HOST_CONFIG_DATAEXCHANGE_STOP_PRODUCERS, true);

	// Create default settings and send them to the device.
	sendDefaultConfiguration(moduleData);

	// Start data acquisition.
	bool ret = caerDeviceDataStart(moduleData->moduleState, &dvMainloopDataNotifyIncrease,
		&dvMainloopDataNotifyDecrease, NULL, &moduleShutdownNotify, moduleData->moduleNode);

	if (!ret) {
		// Failed to start data acquisition, close device and exit.
		caerDeviceClose((caerDeviceHandle *) &moduleData->moduleState);

		return (false);
	}

	// Add config listeners last, to avoid having them dangling if Init doesn't succeed.
	dvConfigNode biasNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "bias/");
	dvConfigNodeAddAttributeListener(biasNode, moduleData, &biasConfigListener);

	dvConfigNode dvsNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "dvs/");
	dvConfigNodeAddAttributeListener(dvsNode, moduleData, &dvsConfigListener);

	dvConfigNode usbNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "usb/");
	dvConfigNodeAddAttributeListener(usbNode, moduleData, &usbConfigListener);

	dvConfigNode sysNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "system/");
	dvConfigNodeAddAttributeListener(sysNode, moduleData, &systemConfigListener);

	dvConfigNodeAddAttributeListener(moduleData->moduleNode, moduleData, &logLevelListener);

	return (true);
}

static void caerInputDVS128Exit(dvModuleData moduleData) {
	// Remove listener, which can reference invalid memory in userData.
	dvConfigNodeRemoveAttributeListener(moduleData->moduleNode, moduleData, &logLevelListener);

	dvConfigNode biasNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "bias/");
	dvConfigNodeRemoveAttributeListener(biasNode, moduleData, &biasConfigListener);

	dvConfigNode dvsNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "dvs/");
	dvConfigNodeRemoveAttributeListener(dvsNode, moduleData, &dvsConfigListener);

	dvConfigNode usbNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "usb/");
	dvConfigNodeRemoveAttributeListener(usbNode, moduleData, &usbConfigListener);

	dvConfigNode sysNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "system/");
	dvConfigNodeRemoveAttributeListener(sysNode, moduleData, &systemConfigListener);

	caerDeviceDataStop(moduleData->moduleState);

	caerDeviceClose((caerDeviceHandle *) &moduleData->moduleState);

	// Clear sourceInfo node.
	dvConfigNode sourceInfoNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "sourceInfo/");
	dvConfigNodeRemoveAllAttributes(sourceInfoNode);

	if (dvConfigNodeGetBool(moduleData->moduleNode, "autoRestart")) {
		// Prime input module again so that it will try to restart if new devices detected.
		dvConfigNodePutBool(moduleData->moduleNode, "running", true);
	}
}

static void caerInputDVS128Run(dvModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out) {
	UNUSED_ARGUMENT(in);

	*out = caerDeviceDataGet(moduleData->moduleState);

	if (*out != NULL) {
		// Detect timestamp reset and call all reset functions for processors and outputs.
		caerEventPacketHeader special = caerEventPacketContainerGetEventPacket(*out, SPECIAL_EVENT);

		if ((special != NULL) && (caerEventPacketHeaderGetEventNumber(special) == 1)
			&& (caerSpecialEventPacketFindValidEventByTypeConst((caerSpecialEventPacketConst) special, TIMESTAMP_RESET)
				   != NULL)) {
			// Update master/slave information.
			struct caer_dvs128_info devInfo = caerDVS128InfoGet(moduleData->moduleState);

			dvConfigNode sourceInfoNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "sourceInfo/");
			dvConfigNodeUpdateReadOnlyAttribute(sourceInfoNode, "deviceIsMaster", DVCFG_TYPE_BOOL,
				(union dvConfigAttributeValue){.boolean = devInfo.deviceIsMaster});
		}
	}
}

static void sendDefaultConfiguration(dvModuleData moduleData) {
	// Send cAER configuration to libcaer and device.
	biasConfigSend(dvConfigNodeGetRelativeNode(moduleData->moduleNode, "bias/"), moduleData);
	systemConfigSend(dvConfigNodeGetRelativeNode(moduleData->moduleNode, "system/"), moduleData);
	usbConfigSend(dvConfigNodeGetRelativeNode(moduleData->moduleNode, "usb/"), moduleData);
	dvsConfigSend(dvConfigNodeGetRelativeNode(moduleData->moduleNode, "dvs/"), moduleData);
}

static void moduleShutdownNotify(void *p) {
	dvConfigNode moduleNode = p;

	// Ensure parent also shuts down (on disconnected device for example).
	dvConfigNodePutBool(moduleNode, "running", false);
}

static void biasConfigSend(dvConfigNode node, dvModuleData moduleData) {
	caerDeviceConfigSet(
		moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_CAS, U32T(dvConfigNodeGetInt(node, "cas")));
	caerDeviceConfigSet(moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_INJGND,
		U32T(dvConfigNodeGetInt(node, "injGnd")));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_REQPD, U32T(dvConfigNodeGetInt(node, "reqPd")));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_PUX, U32T(dvConfigNodeGetInt(node, "puX")));
	caerDeviceConfigSet(moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_DIFFOFF,
		U32T(dvConfigNodeGetInt(node, "diffOff")));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_REQ, U32T(dvConfigNodeGetInt(node, "req")));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_REFR, U32T(dvConfigNodeGetInt(node, "refr")));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_PUY, U32T(dvConfigNodeGetInt(node, "puY")));
	caerDeviceConfigSet(moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_DIFFON,
		U32T(dvConfigNodeGetInt(node, "diffOn")));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_DIFF, U32T(dvConfigNodeGetInt(node, "diff")));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_FOLL, U32T(dvConfigNodeGetInt(node, "foll")));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_PR, U32T(dvConfigNodeGetInt(node, "pr")));
}

static void biasConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	dvModuleData moduleData = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "cas")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_CAS, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "injGnd")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_INJGND, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "reqPd")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_REQPD, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "puX")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_PUX, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "diffOff")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_DIFFOFF, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "req")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_REQ, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "refr")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_REFR, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "puY")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_PUY, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "diffOn")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_DIFFON, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "diff")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_DIFF, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "foll")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_FOLL, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "pr")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_PR, U32T(changeValue.iint));
		}
	}
}

static void dvsConfigSend(dvConfigNode node, dvModuleData moduleData) {
	caerDeviceConfigSet(moduleData->moduleState, DVS128_CONFIG_DVS, DVS128_CONFIG_DVS_ARRAY_RESET,
		dvConfigNodeGetBool(node, "ArrayReset"));
	caerDeviceConfigSet(moduleData->moduleState, DVS128_CONFIG_DVS, DVS128_CONFIG_DVS_TIMESTAMP_RESET,
		dvConfigNodeGetBool(node, "TimestampReset"));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS128_CONFIG_DVS, DVS128_CONFIG_DVS_RUN, dvConfigNodeGetBool(node, "Run"));
}

static void dvsConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	dvModuleData moduleData = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "ArrayReset") && changeValue.boolean) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_DVS, DVS128_CONFIG_DVS_ARRAY_RESET, changeValue.boolean);

			dvConfigNodeAttributeButtonReset(node, changeKey);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "TimestampReset") && changeValue.boolean) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS128_CONFIG_DVS, DVS128_CONFIG_DVS_TIMESTAMP_RESET, changeValue.boolean);

			dvConfigNodeAttributeButtonReset(node, changeKey);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "Run")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS128_CONFIG_DVS, DVS128_CONFIG_DVS_RUN, changeValue.boolean);
		}
	}
}

static void usbConfigSend(dvConfigNode node, dvModuleData moduleData) {
	caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_USB, CAER_HOST_CONFIG_USB_BUFFER_NUMBER,
		U32T(dvConfigNodeGetInt(node, "BufferNumber")));
	caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_USB, CAER_HOST_CONFIG_USB_BUFFER_SIZE,
		U32T(dvConfigNodeGetInt(node, "BufferSize")));
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
	}
}

static void systemConfigSend(dvConfigNode node, dvModuleData moduleData) {
	caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_PACKETS,
		CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_PACKET_SIZE,
		U32T(dvConfigNodeGetInt(node, "PacketContainerMaxPacketSize")));
	caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_PACKETS,
		CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_INTERVAL, U32T(dvConfigNodeGetInt(node, "PacketContainerInterval")));

	// Changes only take effect on module start!
	caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_DATAEXCHANGE,
		CAER_HOST_CONFIG_DATAEXCHANGE_BUFFER_SIZE, U32T(dvConfigNodeGetInt(node, "DataExchangeBufferSize")));
}

static void systemConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	dvModuleData moduleData = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "PacketContainerMaxPacketSize")) {
			caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_PACKETS,
				CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_PACKET_SIZE, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "PacketContainerInterval")) {
			caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_PACKETS,
				CAER_HOST_CONFIG_PACKETS_MAX_CONTAINER_INTERVAL, U32T(changeValue.iint));
		}
	}
}

static void logLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	dvModuleData moduleData = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "logLevel")) {
		caerDeviceConfigSet(
			moduleData->moduleState, CAER_HOST_CONFIG_LOG, CAER_HOST_CONFIG_LOG_LEVEL, U32T(changeValue.iint));
	}
}
