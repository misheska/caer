#include <libcaer/events/imu6.h>
#include <libcaer/events/packetContainer.h>
#include <libcaer/events/polarity.h>
#include <libcaer/events/special.h>

#include <libcaer/devices/dvs132s.h>

#include <dv-sdk/cross/c11threads_posix.h>
#include <dv-sdk/mainloop.h>

static void caerInputDVS132SConfigInit(dvConfigNode moduleNode);
static bool caerInputDVS132SInit(dvModuleData moduleData);
static void caerInputDVS132SRun(dvModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out);
static void caerInputDVS132SExit(dvModuleData moduleData);

static const struct dvModuleFunctionsS DVS132SFunctions = {
	.moduleConfigInit = &caerInputDVS132SConfigInit,
	.moduleInit       = &caerInputDVS132SInit,
	.moduleRun        = &caerInputDVS132SRun,
	.moduleConfig     = NULL,
	.moduleExit       = &caerInputDVS132SExit,
};

static const struct caer_event_stream_out DVS132SOutputs[] = {
	{.type = SPECIAL_EVENT},
	{.type = POLARITY_EVENT},
	{.type = IMU6_EVENT},
};

static const struct dvModuleInfoS DVS132SInfo = {
	.version           = 1,
	.description       = "Connects to a DVS132S camera to get data.",
	.type              = DV_MODULE_INPUT,
	.memSize           = 0,
	.functions         = &DVS132SFunctions,
	.inputStreams      = NULL,
	.inputStreamsSize  = 0,
	.outputStreams     = DVS132SOutputs,
	.outputStreamsSize = CAER_EVENT_STREAM_OUT_SIZE(DVS132SOutputs),
};

dvModuleInfo dvModuleGetInfo(void) {
	return (&DVS132SInfo);
}

static void moduleShutdownNotify(void *p);

static void createDefaultBiasConfiguration(dvModuleData moduleData);
static void createDefaultLogicConfiguration(dvModuleData moduleData, const struct caer_dvs132s_info *devInfo);
static void createDefaultUSBConfiguration(dvModuleData moduleData);
static void sendDefaultConfiguration(dvModuleData moduleData, const struct caer_dvs132s_info *devInfo);

static void biasConfigSend(dvConfigNode node, dvModuleData moduleData);
static void biasConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void muxConfigSend(dvConfigNode node, dvModuleData moduleData);
static void muxConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void dvsConfigSend(dvConfigNode node, dvModuleData moduleData);
static void dvsConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void imuConfigSend(dvConfigNode node, dvModuleData moduleData);
static void imuConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void extInputConfigSend(dvConfigNode node, dvModuleData moduleData, const struct caer_dvs132s_info *devInfo);
static void extInputConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void systemConfigSend(dvConfigNode node, dvModuleData moduleData);
static void usbConfigSend(dvConfigNode node, dvModuleData moduleData);
static void usbConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void systemConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void logLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

static union dvConfigAttributeValue statisticsUpdater(void *userData, const char *key, enum dvConfigAttributeType type);

static void caerInputDVS132SConfigInit(dvConfigNode moduleNode) {
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

	dvConfigNode sysNode = dvConfigNodeGetRelativeNode(moduleNode, "system/");

	// Packet settings (size (in events) and time interval (in µs)).
	dvConfigNodeCreateInt(sysNode, "PacketContainerMaxPacketSize", 0, 0, 10 * 1024 * 1024, DVCFG_FLAGS_NORMAL,
		"Maximum packet size in events, when any packet reaches "
		"this size, the EventPacketContainer is sent for "
		"processing.");
	dvConfigNodeCreateInt(sysNode, "PacketContainerInterval", 10000, 1, 120 * 1000 * 1000, DVCFG_FLAGS_NORMAL,
		"Time interval in µs, each sent EventPacketContainer will "
		"span this interval.");

	// Ring-buffer setting (only changes value on module init/shutdown cycles).
	dvConfigNodeCreateInt(sysNode, "DataExchangeBufferSize", 64, 8, 1024, DVCFG_FLAGS_NORMAL,
		"Size of EventPacketContainer queue, used for transfers "
		"between data acquisition thread and mainloop.");
}

static bool caerInputDVS132SInit(dvModuleData moduleData) {
	dvModuleLog(moduleData, CAER_LOG_DEBUG, "Initializing module ...");

	// Start data acquisition, and correctly notify mainloop of new data and
	// module of exceptional shutdown cases (device pulled, ...).
	char *serialNumber      = dvConfigNodeGetString(moduleData->moduleNode, "serialNumber");
	moduleData->moduleState = caerDeviceOpen(U16T(moduleData->moduleID), CAER_DEVICE_DVS132S,
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
	struct caer_dvs132s_info devInfo = caerDVS132SInfoGet(moduleData->moduleState);

	dvConfigNode sourceInfoNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "sourceInfo/");

	dvConfigNodeCreateInt(sourceInfoNode, "firmwareVersion", devInfo.firmwareVersion, devInfo.firmwareVersion,
		devInfo.firmwareVersion, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Device USB firmware version.");
	dvConfigNodeCreateInt(sourceInfoNode, "logicVersion", devInfo.logicVersion, devInfo.logicVersion,
		devInfo.logicVersion, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Device FPGA logic version.");
	dvConfigNodeCreateInt(sourceInfoNode, "chipID", devInfo.chipID, devInfo.chipID, devInfo.chipID,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Device chip identification number.");

	dvConfigNodeCreateBool(sourceInfoNode, "deviceIsMaster", devInfo.deviceIsMaster,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Timestamp synchronization support: device master status.");
	dvConfigNodeCreateInt(sourceInfoNode, "polaritySizeX", devInfo.dvsSizeX, devInfo.dvsSizeX, devInfo.dvsSizeX,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Polarity events width.");
	dvConfigNodeCreateInt(sourceInfoNode, "polaritySizeY", devInfo.dvsSizeY, devInfo.dvsSizeY, devInfo.dvsSizeY,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Polarity events height.");

	// Extra features.
	dvConfigNodeCreateBool(sourceInfoNode, "muxHasStatistics", devInfo.muxHasStatistics,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT,
		"Device supports FPGA Multiplexer statistics (USB event drops).");
	dvConfigNodeCreateBool(sourceInfoNode, "extInputHasGenerator", devInfo.extInputHasGenerator,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Device supports generating pulses on output signal connector.");
	dvConfigNodeCreateBool(sourceInfoNode, "dvsHasStatistics", devInfo.dvsHasStatistics,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Device supports FPGA DVS statistics.");

	// Put source information for generic visualization, to be used to display and
	// debug filter information.
	int16_t dataSizeX = devInfo.dvsSizeX;
	int16_t dataSizeY = devInfo.dvsSizeY;

	dvConfigNodeCreateInt(sourceInfoNode, "dataSizeX", dataSizeX, dataSizeX, dataSizeX,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Data width.");
	dvConfigNodeCreateInt(sourceInfoNode, "dataSizeY", dataSizeY, dataSizeY, dataSizeY,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Data height.");

	// Generate source string for output modules.
	size_t sourceStringLength
		= (size_t) snprintf(NULL, 0, "#Source %" PRIu16 ": %s\r\n", moduleData->moduleID, "DVS132S");

	char sourceString[sourceStringLength + 1];
	snprintf(sourceString, sourceStringLength + 1, "#Source %" PRIu16 ": %s\r\n", moduleData->moduleID, "DVS132S");
	sourceString[sourceStringLength] = '\0';

	dvConfigNodeCreateString(sourceInfoNode, "sourceString", sourceString, sourceStringLength, sourceStringLength,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Device source information.");

	// Generate sub-system string for module.
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

	// Create default settings.
	createDefaultBiasConfiguration(moduleData);
	createDefaultLogicConfiguration(moduleData, &devInfo);
	createDefaultUSBConfiguration(moduleData);

	// Start data acquisition.
	bool ret = caerDeviceDataStart(moduleData->moduleState, &dvMainloopDataNotifyIncrease,
		&dvMainloopDataNotifyDecrease, NULL, &moduleShutdownNotify, moduleData->moduleNode);

	if (!ret) {
		// Failed to start data acquisition, close device and exit.
		caerDeviceClose((caerDeviceHandle *) &moduleData->moduleState);

		return (false);
	}

	// Send configuration, enabling data capture as requested.
	sendDefaultConfiguration(moduleData, &devInfo);

	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "DVS132S/");

	// Add config listeners last, to avoid having them dangling if Init doesn't succeed.
	dvConfigNode biasNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "bias/");
	dvConfigNodeAddAttributeListener(biasNode, moduleData, &biasConfigListener);

	dvConfigNode muxNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "multiplexer/");
	dvConfigNodeAddAttributeListener(muxNode, moduleData, &muxConfigListener);

	dvConfigNode dvsNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "dvs/");
	dvConfigNodeAddAttributeListener(dvsNode, moduleData, &dvsConfigListener);

	dvConfigNode imuNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "imu/");
	dvConfigNodeAddAttributeListener(imuNode, moduleData, &imuConfigListener);

	dvConfigNode extNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "externalInput/");
	dvConfigNodeAddAttributeListener(extNode, moduleData, &extInputConfigListener);

	dvConfigNode usbNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "usb/");
	dvConfigNodeAddAttributeListener(usbNode, moduleData, &usbConfigListener);

	dvConfigNode sysNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "system/");
	dvConfigNodeAddAttributeListener(sysNode, moduleData, &systemConfigListener);

	dvConfigNodeAddAttributeListener(moduleData->moduleNode, moduleData, &logLevelListener);

	return (true);
}

static void caerInputDVS132SRun(dvModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out) {
	UNUSED_ARGUMENT(in);

	*out = caerDeviceDataGet(moduleData->moduleState);

	if (*out != NULL) {
		// Detect timestamp reset and call all reset functions for processors and outputs.
		caerEventPacketHeader special = caerEventPacketContainerGetEventPacket(*out, SPECIAL_EVENT);

		if ((special != NULL) && (caerEventPacketHeaderGetEventNumber(special) == 1)
			&& (caerSpecialEventPacketFindValidEventByTypeConst((caerSpecialEventPacketConst) special, TIMESTAMP_RESET)
				   != NULL)) {
			// Update master/slave information.
			struct caer_dvs132s_info devInfo = caerDVS132SInfoGet(moduleData->moduleState);

			dvConfigNode sourceInfoNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "sourceInfo/");
			dvConfigNodeUpdateReadOnlyAttribute(sourceInfoNode, "deviceIsMaster", DVCFG_TYPE_BOOL,
				(union dvConfigAttributeValue){.boolean = devInfo.deviceIsMaster});
		}
	}
}

static void caerInputDVS132SExit(dvModuleData moduleData) {
	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "DVS132S/");

	// Remove listener, which can reference invalid memory in userData.
	dvConfigNodeRemoveAttributeListener(moduleData->moduleNode, moduleData, &logLevelListener);

	dvConfigNode biasNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "bias/");
	dvConfigNodeRemoveAttributeListener(biasNode, moduleData, &biasConfigListener);

	dvConfigNode muxNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "multiplexer/");
	dvConfigNodeRemoveAttributeListener(muxNode, moduleData, &muxConfigListener);

	dvConfigNode dvsNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "dvs/");
	dvConfigNodeRemoveAttributeListener(dvsNode, moduleData, &dvsConfigListener);

	dvConfigNode imuNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "imu/");
	dvConfigNodeRemoveAttributeListener(imuNode, moduleData, &imuConfigListener);

	dvConfigNode extNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "externalInput/");
	dvConfigNodeRemoveAttributeListener(extNode, moduleData, &extInputConfigListener);

	dvConfigNode usbNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "usb/");
	dvConfigNodeRemoveAttributeListener(usbNode, moduleData, &usbConfigListener);

	dvConfigNode sysNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "system/");
	dvConfigNodeRemoveAttributeListener(sysNode, moduleData, &systemConfigListener);

	// Remove statistics read modifiers.
	dvConfigNode statNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "statistics/");
	dvConfigNodeAttributeUpdaterRemoveAll(statNode);

	caerDeviceDataStop(moduleData->moduleState);

	caerDeviceClose((caerDeviceHandle *) &moduleData->moduleState);

	// Clear sourceInfo node.
	dvConfigNode sourceInfoNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "sourceInfo/");
	dvConfigNodeRemoveAllAttributes(sourceInfoNode);

	if (dvConfigNodeGetBool(moduleData->moduleNode, "autoRestart")) {
		// Prime input module again so that it will try to restart if new devices
		// detected.
		dvConfigNodePutBool(moduleData->moduleNode, "running", true);
	}
}

static void moduleShutdownNotify(void *p) {
	dvConfigNode moduleNode = p;

	// Ensure parent also shuts down (on disconnected device for example).
	dvConfigNodePutBool(moduleNode, "running", false);
}

static void createDefaultBiasConfiguration(dvModuleData moduleData) {
	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "DVS132S/");

	// Chip biases, based on testing defaults.
	dvConfigNode biasNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "bias/");

	dvConfigNodeCreateBool(biasNode, "BiasEnable", true, DVCFG_FLAGS_NORMAL, "Enable bias generator to power chip.");

	dvConfigNodeCreateInt(
		biasNode, "PrBp", 100 * 1000, 0, 1000000, DVCFG_FLAGS_NORMAL, "Bias PrBp (in pAmp) - Photoreceptor bandwidth.");

	dvConfigNodeCreateInt(
		biasNode, "PrSFBpCoarse", 1, 0, 1023, DVCFG_FLAGS_NORMAL, "Bias PrSFBp (in pAmp) - Photoreceptor bandwidth.");
	dvConfigNodeCreateInt(
		biasNode, "PrSFBpFine", 1, 0, 1023, DVCFG_FLAGS_NORMAL, "Bias PrSFBp (in pAmp) - Photoreceptor bandwidth.");

	dvConfigNodeCreateInt(
		biasNode, "BlPuBp", 0, 0, 1000000, DVCFG_FLAGS_NORMAL, "Bias BlPuBp (in pAmp) - Bitline pull-up strength.");
	dvConfigNodeCreateInt(biasNode, "BiasBufBp", 10 * 1000, 0, 1000000, DVCFG_FLAGS_NORMAL,
		"Bias BiasBufBp (in pAmp) - P type bias buffer strength.");
	dvConfigNodeCreateInt(
		biasNode, "OffBn", 200, 0, 1000000, DVCFG_FLAGS_NORMAL, "Bias OffBn (in pAmp) - Comparator OFF threshold.");
	dvConfigNodeCreateInt(biasNode, "DiffBn", 10 * 1000, 0, 1000000, DVCFG_FLAGS_NORMAL,
		"Bias DiffBn (in pAmp) - Delta amplifier strength.");
	dvConfigNodeCreateInt(
		biasNode, "OnBn", 400 * 1000, 0, 1000000, DVCFG_FLAGS_NORMAL, "Bias OnBn (in pAmp) - Comparator ON threshold.");
	dvConfigNodeCreateInt(biasNode, "CasBn", 400 * 1000, 0, 1000000, DVCFG_FLAGS_NORMAL,
		"Bias CasBn (in pAmp) - Cascode for delta amplifier and comparator.");
	dvConfigNodeCreateInt(biasNode, "DPBn", 100 * 1000, 0, 1000000, DVCFG_FLAGS_NORMAL,
		"Bias DPBn (in pAmp) - In-pixel direct path current limit.");
	dvConfigNodeCreateInt(biasNode, "BiasBufBn", 10 * 1000, 0, 1000000, DVCFG_FLAGS_NORMAL,
		"Bias BiasBufBn (in pAmp) - N type bias buffer strength.");
	dvConfigNodeCreateInt(biasNode, "ABufBn", 0, 0, 1000000, DVCFG_FLAGS_NORMAL,
		"Bias ABufBn (in pAmp) - Diagnostic analog buffer strength.");
}

static void createDefaultLogicConfiguration(dvModuleData moduleData, const struct caer_dvs132s_info *devInfo) {
	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "DVS132S/");

	// Subsystem 0: Multiplexer
	dvConfigNode muxNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "multiplexer/");

	dvConfigNodeCreateBool(muxNode, "Run", true, DVCFG_FLAGS_NORMAL, "Enable multiplexer state machine.");
	dvConfigNodeCreateBool(muxNode, "TimestampRun", true, DVCFG_FLAGS_NORMAL, "Enable µs-timestamp generation.");
	dvConfigNodeCreateBool(muxNode, "TimestampReset", false, DVCFG_FLAGS_NORMAL, "Reset timestamps to zero.");
	dvConfigNodeAttributeModifierButton(muxNode, "TimestampReset", "EXECUTE");
	dvConfigNodeCreateBool(muxNode, "RunChip", true, DVCFG_FLAGS_NORMAL, "Enable the chip's bias generator.");
	dvConfigNodeCreateBool(
		muxNode, "DropDVSOnTransferStall", false, DVCFG_FLAGS_NORMAL, "Drop Polarity events when USB FIFO is full.");
	dvConfigNodeCreateBool(muxNode, "DropExtInputOnTransferStall", true, DVCFG_FLAGS_NORMAL,
		"Drop ExternalInput events when USB FIFO is full.");

	// Subsystem 1: DVS
	dvConfigNode dvsNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "dvs/");

	dvConfigNodeCreateBool(dvsNode, "Run", true, DVCFG_FLAGS_NORMAL, "Enable DVS (Polarity events).");
	dvConfigNodeCreateBool(
		dvsNode, "WaitOnTransferStall", true, DVCFG_FLAGS_NORMAL, "On event FIFO full, pause readout.");
	dvConfigNodeCreateBool(dvsNode, "FilterAtLeast2Unsigned", false, DVCFG_FLAGS_NORMAL,
		"Only read events from a group of four pixels if at least two are active, regardless of polarity.");
	dvConfigNodeCreateBool(dvsNode, "FilterNotAll4Unsigned", false, DVCFG_FLAGS_NORMAL,
		"Only read events from a group of four pixels if not all four are active, regardless of polarity.");
	dvConfigNodeCreateBool(dvsNode, "FilterAtLeast2Signed", false, DVCFG_FLAGS_NORMAL,
		"Only read events from a group of four pixels if at least two are active and have the same polarity.");
	dvConfigNodeCreateBool(dvsNode, "FilterNotAll4Signed", false, DVCFG_FLAGS_NORMAL,
		"Only read events from a group of four pixels if not all four are active and have the same polarity.");
	dvConfigNodeCreateInt(
		dvsNode, "RestartTime", 100, 1, ((0x01 << 7) - 1), DVCFG_FLAGS_NORMAL, "Restart pulse length, in us.");
	dvConfigNodeCreateInt(dvsNode, "CaptureInterval", 500, 1, ((0x01 << 21) - 1), DVCFG_FLAGS_NORMAL,
		"Time interval between DVS readouts, in us.");
	dvConfigNodeCreateString(dvsNode, "RowEnable", "111111111111111111111111111111111111111111111111111111111111111111",
		66, 66, DVCFG_FLAGS_NORMAL, "Enable rows to be read-out (ROI filter).");
	dvConfigNodeCreateString(dvsNode, "ColumnEnable", "1111111111111111111111111111111111111111111111111111", 52, 52,
		DVCFG_FLAGS_NORMAL, "Enable columns to be read-out (ROI filter).");

	// Subsystem 3: IMU
	dvConfigNode imuNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "imu/");

	dvConfigNodeCreateBool(imuNode, "RunAccelerometer", true, DVCFG_FLAGS_NORMAL, "Enable accelerometer.");
	dvConfigNodeCreateBool(imuNode, "RunGyroscope", true, DVCFG_FLAGS_NORMAL, "Enable gyroscope.");
	dvConfigNodeCreateBool(imuNode, "RunTemperature", true, DVCFG_FLAGS_NORMAL, "Enable temperature sensor.");
	dvConfigNodeCreateInt(
		imuNode, "AccelDataRate", 6, 0, 7, DVCFG_FLAGS_NORMAL, "Accelerometer bandwidth configuration.");
	dvConfigNodeCreateInt(imuNode, "AccelFilter", 2, 0, 2, DVCFG_FLAGS_NORMAL, "Accelerometer filter configuration.");
	dvConfigNodeCreateInt(imuNode, "AccelRange", 1, 0, 3, DVCFG_FLAGS_NORMAL, "Accelerometer range configuration.");
	dvConfigNodeCreateInt(imuNode, "GyroDataRate", 5, 0, 7, DVCFG_FLAGS_NORMAL, "Gyroscope bandwidth configuration.");
	dvConfigNodeCreateInt(imuNode, "GyroFilter", 2, 0, 2, DVCFG_FLAGS_NORMAL, "Gyroscope filter configuration.");
	dvConfigNodeCreateInt(imuNode, "GyroRange", 2, 0, 4, DVCFG_FLAGS_NORMAL, "Gyroscope range configuration.");

	// Subsystem 4: External Input
	dvConfigNode extNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "externalInput/");

	dvConfigNodeCreateBool(extNode, "RunDetector", false, DVCFG_FLAGS_NORMAL, "Enable signal detector 0.");
	dvConfigNodeCreateBool(
		extNode, "DetectRisingEdges", false, DVCFG_FLAGS_NORMAL, "Emit special event if a rising edge is detected.");
	dvConfigNodeCreateBool(
		extNode, "DetectFallingEdges", false, DVCFG_FLAGS_NORMAL, "Emit special event if a falling edge is detected.");
	dvConfigNodeCreateBool(
		extNode, "DetectPulses", true, DVCFG_FLAGS_NORMAL, "Emit special event if a pulse is detected.");
	dvConfigNodeCreateBool(
		extNode, "DetectPulsePolarity", true, DVCFG_FLAGS_NORMAL, "Polarity of the pulse to be detected.");
	dvConfigNodeCreateInt(extNode, "DetectPulseLength", 10, 1, ((0x01 << 20) - 1), DVCFG_FLAGS_NORMAL,
		"Minimal length of the pulse to be detected (in µs).");

	if (devInfo->extInputHasGenerator) {
		dvConfigNodeCreateBool(
			extNode, "RunGenerator", false, DVCFG_FLAGS_NORMAL, "Enable signal generator (PWM-like).");
		dvConfigNodeCreateBool(
			extNode, "GeneratePulsePolarity", true, DVCFG_FLAGS_NORMAL, "Polarity of the generated pulse.");
		dvConfigNodeCreateInt(extNode, "GeneratePulseInterval", 10, 1, ((0x01 << 20) - 1), DVCFG_FLAGS_NORMAL,
			"Time interval between consecutive pulses (in µs).");
		dvConfigNodeCreateInt(extNode, "GeneratePulseLength", 5, 1, ((0x01 << 20) - 1), DVCFG_FLAGS_NORMAL,
			"Time length of a pulse (in µs).");
		dvConfigNodeCreateBool(extNode, "GenerateInjectOnRisingEdge", false, DVCFG_FLAGS_NORMAL,
			"Emit a special event when a rising edge is generated.");
		dvConfigNodeCreateBool(extNode, "GenerateInjectOnFallingEdge", false, DVCFG_FLAGS_NORMAL,
			"Emit a special event when a falling edge is generated.");
	}

	// Device event statistics.
	if (devInfo->muxHasStatistics) {
		dvConfigNode statNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "statistics/");

		dvConfigNodeCreateLong(statNode, "muxDroppedDVS", 0, 0, INT64_MAX,
			DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Number of dropped DVS events due to USB full.");
		dvConfigNodeAttributeUpdaterAdd(
			statNode, "muxDroppedDVS", DVCFG_TYPE_LONG, &statisticsUpdater, moduleData->moduleState, false);

		dvConfigNodeCreateLong(statNode, "muxDroppedExtInput", 0, 0, INT64_MAX,
			DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Number of dropped External Input events due to USB full.");
		dvConfigNodeAttributeUpdaterAdd(
			statNode, "muxDroppedExtInput", DVCFG_TYPE_LONG, &statisticsUpdater, moduleData->moduleState, false);
	}

	if (devInfo->dvsHasStatistics) {
		dvConfigNode statNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "statistics/");

		dvConfigNodeCreateLong(statNode, "dvsTransactionsSuccess", 0, 0, INT64_MAX,
			DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Number of groups of events received successfully.");
		dvConfigNodeAttributeUpdaterAdd(
			statNode, "dvsTransactionsSuccess", DVCFG_TYPE_LONG, &statisticsUpdater, moduleData->moduleState, false);

		dvConfigNodeCreateLong(statNode, "dvsTransactionsSkipped", 0, 0, INT64_MAX,
			DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Number of dropped groups of events due to full buffers.");
		dvConfigNodeAttributeUpdaterAdd(
			statNode, "dvsTransactionsSkipped", DVCFG_TYPE_LONG, &statisticsUpdater, moduleData->moduleState, false);

		dvConfigNodeCreateLong(statNode, "dvsTransactionsAll", 0, 0, INT64_MAX,
			DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Number of dropped groups of events due to full buffers.");
		dvConfigNodeAttributeUpdaterAdd(
			statNode, "dvsTransactionsAll", DVCFG_TYPE_LONG, &statisticsUpdater, moduleData->moduleState, false);

		dvConfigNodeCreateLong(statNode, "dvsTransactionsErrored", 0, 0, INT64_MAX,
			DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Number of erroneous groups of events.");
		dvConfigNodeAttributeUpdaterAdd(
			statNode, "dvsTransactionsErrored", DVCFG_TYPE_LONG, &statisticsUpdater, moduleData->moduleState, false);
	}
}

static void createDefaultUSBConfiguration(dvModuleData moduleData) {
	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "DVS132S/");

	// Subsystem 9: FX2/3 USB Configuration and USB buffer settings.
	dvConfigNode usbNode = dvConfigNodeGetRelativeNode(deviceConfigNode, "usb/");
	dvConfigNodeCreateBool(
		usbNode, "Run", true, DVCFG_FLAGS_NORMAL, "Enable the USB state machine (FPGA to USB data exchange).");
	dvConfigNodeCreateInt(usbNode, "EarlyPacketDelay", 8, 1, 8000, DVCFG_FLAGS_NORMAL,
		"Send early USB packets if this timeout is reached (in 125µs time-slices).");

	dvConfigNodeCreateInt(usbNode, "BufferNumber", 8, 2, 128, DVCFG_FLAGS_NORMAL, "Number of USB transfers.");
	dvConfigNodeCreateInt(usbNode, "BufferSize", 8192, 512, 32768, DVCFG_FLAGS_NORMAL,
		"Size in bytes of data buffers for USB transfers.");
}

static void sendDefaultConfiguration(dvModuleData moduleData, const struct caer_dvs132s_info *devInfo) {
	// Device related configuration has its own sub-node.
	dvConfigNode deviceConfigNode = dvConfigNodeGetRelativeNode(moduleData->moduleNode, "DVS132S/");

	// Send cAER configuration to libcaer and device.
	biasConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "bias/"), moduleData);

	// Wait 200 ms for biases to stabilize.
	struct timespec biasEnSleep = {.tv_sec = 0, .tv_nsec = 200000000};
	thrd_sleep(&biasEnSleep, NULL);

	systemConfigSend(dvConfigNodeGetRelativeNode(moduleData->moduleNode, "system/"), moduleData);
	usbConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "usb/"), moduleData);
	muxConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "multiplexer/"), moduleData);

	// Wait 50 ms for data transfer to be ready.
	struct timespec noDataSleep = {.tv_sec = 0, .tv_nsec = 50000000};
	thrd_sleep(&noDataSleep, NULL);

	dvsConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "dvs/"), moduleData);
	imuConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "imu/"), moduleData);
	extInputConfigSend(dvConfigNodeGetRelativeNode(deviceConfigNode, "externalInput/"), moduleData, devInfo);
}

static void biasConfigSend(dvConfigNode node, dvModuleData moduleData) {
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_PRBP,
		caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(dvConfigNodeGetInt(node, "PrBp")))));

	struct caer_bias_coarsefine1024 prSF;
	prSF.coarseValue = U16T(dvConfigNodeGetInt(node, "PrSFBpCoarse"));
	prSF.fineValue   = U16T(dvConfigNodeGetInt(node, "PrSFBpFine"));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_PRSFBP, caerBiasCoarseFine1024Generate(prSF));

	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_BLPUBP,
		caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(dvConfigNodeGetInt(node, "BlPuBp")))));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_BIASBUFBP,
		caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(dvConfigNodeGetInt(node, "BiasBufBp")))));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_CASBN,
		caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(dvConfigNodeGetInt(node, "CasBn")))));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_DPBN,
		caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(dvConfigNodeGetInt(node, "DPBn")))));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_BIASBUFBN,
		caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(dvConfigNodeGetInt(node, "BiasBufBn")))));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_ABUFBN,
		caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(dvConfigNodeGetInt(node, "ABufBn")))));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_OFFBN,
		caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(dvConfigNodeGetInt(node, "OffBn")))));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_DIFFBN,
		caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(dvConfigNodeGetInt(node, "DiffBn")))));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_ONBN,
		caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(dvConfigNodeGetInt(node, "OnBn")))));

	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_RUN_CHIP,
		dvConfigNodeGetBool(node, "BiasEnable"));
}

static void biasConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	dvModuleData moduleData = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "PrBp")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_PRBP,
				caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(changeValue.iint))));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "PrSFBpCoarse")) {
			struct caer_bias_coarsefine1024 prSF;
			prSF.coarseValue = U16T(changeValue.iint);
			prSF.fineValue   = U16T(dvConfigNodeGetInt(node, "PrSFBpFine"));
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_PRSFBP,
				caerBiasCoarseFine1024Generate(prSF));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "PrSFBpFine")) {
			struct caer_bias_coarsefine1024 prSF;
			prSF.coarseValue = U16T(dvConfigNodeGetInt(node, "PrSFBpCoarse"));
			prSF.fineValue   = U16T(changeValue.iint);
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_PRSFBP,
				caerBiasCoarseFine1024Generate(prSF));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "BlPuBp")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_BLPUBP,
				caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(changeValue.iint))));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "BiasBufBp")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_BIASBUFBP,
				caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(changeValue.iint))));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "OffBn")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_OFFBN,
				caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(changeValue.iint))));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "DiffBn")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_DIFFBN,
				caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(changeValue.iint))));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "OnBn")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_ONBN,
				caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(changeValue.iint))));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "CasBn")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_CASBN,
				caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(changeValue.iint))));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "DPBn")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_DPBN,
				caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(changeValue.iint))));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "BiasBufBn")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_BIASBUFBN,
				caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(changeValue.iint))));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "ABufBn")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_BIAS, DVS132S_CONFIG_BIAS_ABUFBN,
				caerBiasCoarseFine1024Generate(caerBiasCoarseFine1024FromCurrent(U32T(changeValue.iint))));
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "BiasEnable")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_RUN_CHIP, changeValue.boolean);
		}
	}
}

static void muxConfigSend(dvConfigNode node, dvModuleData moduleData) {
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_TIMESTAMP_RESET,
		dvConfigNodeGetBool(node, "TimestampReset"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_DROP_DVS_ON_TRANSFER_STALL,
		dvConfigNodeGetBool(node, "DropDVSOnTransferStall"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_DROP_EXTINPUT_ON_TRANSFER_STALL,
		dvConfigNodeGetBool(node, "DropExtInputOnTransferStall"));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_RUN_CHIP, dvConfigNodeGetBool(node, "RunChip"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_TIMESTAMP_RUN,
		dvConfigNodeGetBool(node, "TimestampRun"));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_RUN, dvConfigNodeGetBool(node, "Run"));
}

static void muxConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	dvModuleData moduleData = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "TimestampReset") && changeValue.boolean) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_TIMESTAMP_RESET, changeValue.boolean);

			dvConfigNodePutBool(node, changeKey, false);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "DropDVSOnTransferStall")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_MUX,
				DVS132S_CONFIG_MUX_DROP_DVS_ON_TRANSFER_STALL, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "DropExtInputOnTransferStall")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_MUX,
				DVS132S_CONFIG_MUX_DROP_EXTINPUT_ON_TRANSFER_STALL, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "RunChip")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_RUN_CHIP, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "TimestampRun")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_TIMESTAMP_RUN, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "Run")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_RUN, changeValue.boolean);
		}
	}
}

static inline void dvsRowEnableParse(const char *rowEnableStr, caerDeviceHandle cdh) {
	size_t rowEnableIndex = 0;

	uint32_t rowInt31To0 = 0;

	for (size_t i = 0; i < 32; i++) {
		if (rowEnableStr[rowEnableIndex++] == '1') {
			rowInt31To0 |= U32T(0x01 << i);
		}
	}

	uint32_t rowInt63To32 = 0;

	for (size_t i = 0; i < 32; i++) {
		if (rowEnableStr[rowEnableIndex++] == '1') {
			rowInt63To32 |= U32T(0x01 << i);
		}
	}

	uint32_t rowInt65To64 = 0;

	for (size_t i = 0; i < 2; i++) {
		if (rowEnableStr[rowEnableIndex++] == '1') {
			rowInt65To64 |= U32T(0x01 << i);
		}
	}

	caerDeviceConfigSet(cdh, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_ROW_ENABLE_31_TO_0, rowInt31To0);
	caerDeviceConfigSet(cdh, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_ROW_ENABLE_63_TO_32, rowInt63To32);
	caerDeviceConfigSet(cdh, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_ROW_ENABLE_65_TO_64, rowInt65To64);
}

static inline void dvsColumnEnableParse(const char *columnEnableStr, caerDeviceHandle cdh) {
	size_t columnEnableIndex = 0;

	uint32_t columnInt31To0 = 0;

	for (size_t i = 0; i < 32; i++) {
		if (columnEnableStr[columnEnableIndex++] == '1') {
			columnInt31To0 |= U32T(0x01 << i);
		}
	}

	uint32_t columnInt51To32 = 0;

	for (size_t i = 0; i < 20; i++) {
		if (columnEnableStr[columnEnableIndex++] == '1') {
			columnInt51To32 |= U32T(0x01 << i);
		}
	}

	caerDeviceConfigSet(cdh, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_COLUMN_ENABLE_31_TO_0, columnInt31To0);
	caerDeviceConfigSet(cdh, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_COLUMN_ENABLE_51_TO_32, columnInt51To32);
}

static void dvsConfigSend(dvConfigNode node, dvModuleData moduleData) {
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_WAIT_ON_TRANSFER_STALL,
		dvConfigNodeGetBool(node, "WaitOnTransferStall"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_FILTER_AT_LEAST_2_UNSIGNED,
		dvConfigNodeGetBool(node, "FilterAtLeast2Unsigned"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_FILTER_NOT_ALL_4_UNSIGNED,
		dvConfigNodeGetBool(node, "FilterNotAll4Unsigned"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_FILTER_AT_LEAST_2_SIGNED,
		dvConfigNodeGetBool(node, "FilterAtLeast2Signed"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_FILTER_NOT_ALL_4_SIGNED,
		dvConfigNodeGetBool(node, "FilterNotAll4Signed"));

	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_RESTART_TIME,
		U32T(dvConfigNodeGetInt(node, "RestartTime")));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_CAPTURE_INTERVAL,
		U32T(dvConfigNodeGetInt(node, "CaptureInterval")));

	// Parse string bitfields into corresponding integer bitfields for device.
	char *rowEnableStr = dvConfigNodeGetString(node, "RowEnable");

	dvsRowEnableParse(rowEnableStr, moduleData->moduleState);

	free(rowEnableStr);

	// Parse string bitfields into corresponding integer bitfields for device.
	char *columnEnableStr = dvConfigNodeGetString(node, "ColumnEnable");

	dvsColumnEnableParse(columnEnableStr, moduleData->moduleState);

	free(columnEnableStr);

	// Wait 5 ms for row/column enables to have been sent out.
	struct timespec enableSleep = {.tv_sec = 0, .tv_nsec = 5000000};
	thrd_sleep(&enableSleep, NULL);

	caerDeviceConfigSet(
		moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_RUN, dvConfigNodeGetBool(node, "Run"));
}

static void dvsConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	dvModuleData moduleData = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "WaitOnTransferStall")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_WAIT_ON_TRANSFER_STALL,
				changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "FilterAtLeast2Unsigned")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS,
				DVS132S_CONFIG_DVS_FILTER_AT_LEAST_2_UNSIGNED, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "FilterNotAll4Unsigned")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS,
				DVS132S_CONFIG_DVS_FILTER_NOT_ALL_4_UNSIGNED, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "FilterAtLeast2Signed")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS,
				DVS132S_CONFIG_DVS_FILTER_AT_LEAST_2_SIGNED, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "FilterNotAll4Signed")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_FILTER_NOT_ALL_4_SIGNED,
				changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "RestartTime")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_RESTART_TIME, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "CaptureInterval")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_CAPTURE_INTERVAL,
				U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_STRING && caerStrEquals(changeKey, "RowEnable")) {
			// Parse string bitfields into corresponding integer bitfields for device.
			const char *rowEnableStr = changeValue.string;

			dvsRowEnableParse(rowEnableStr, moduleData->moduleState);
		}
		else if (changeType == DVCFG_TYPE_STRING && caerStrEquals(changeKey, "ColumnEnable")) {
			// Parse string bitfields into corresponding integer bitfields for device.
			const char *columnEnableStr = changeValue.string;

			dvsColumnEnableParse(columnEnableStr, moduleData->moduleState);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "Run")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_RUN, changeValue.boolean);
		}
	}
}

static void imuConfigSend(dvConfigNode node, dvModuleData moduleData) {
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_ACCEL_DATA_RATE,
		U32T(dvConfigNodeGetInt(node, "AccelDataRate")));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_ACCEL_FILTER,
		U32T(dvConfigNodeGetInt(node, "AccelFilter")));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_ACCEL_RANGE,
		U32T(dvConfigNodeGetInt(node, "AccelRange")));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_GYRO_DATA_RATE,
		U32T(dvConfigNodeGetInt(node, "GyroDataRate")));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_GYRO_FILTER,
		U32T(dvConfigNodeGetInt(node, "GyroFilter")));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_GYRO_RANGE,
		U32T(dvConfigNodeGetInt(node, "GyroRange")));

	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_RUN_ACCELEROMETER,
		dvConfigNodeGetBool(node, "RunAccelerometer"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_RUN_GYROSCOPE,
		dvConfigNodeGetBool(node, "RunGyroscope"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_RUN_TEMPERATURE,
		dvConfigNodeGetBool(node, "RunTemperature"));
}

static void imuConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	dvModuleData moduleData = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "AccelDataRate")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_ACCEL_DATA_RATE,
				U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "AccelFilter")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_ACCEL_FILTER, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "AccelRange")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_ACCEL_RANGE, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "GyroDataRate")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_GYRO_DATA_RATE, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "GyroFilter")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_GYRO_FILTER, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "GyroRange")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_GYRO_RANGE, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "RunAccelerometer")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_RUN_ACCELEROMETER, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "RunGyroscope")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_RUN_GYROSCOPE, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "RunTemperature")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_IMU, DVS132S_CONFIG_IMU_RUN_TEMPERATURE, changeValue.boolean);
		}
	}
}

static void extInputConfigSend(dvConfigNode node, dvModuleData moduleData, const struct caer_dvs132s_info *devInfo) {
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT, DVS132S_CONFIG_EXTINPUT_DETECT_RISING_EDGES,
		dvConfigNodeGetBool(node, "DetectRisingEdges"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT, DVS132S_CONFIG_EXTINPUT_DETECT_FALLING_EDGES,
		dvConfigNodeGetBool(node, "DetectFallingEdges"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT, DVS132S_CONFIG_EXTINPUT_DETECT_PULSES,
		dvConfigNodeGetBool(node, "DetectPulses"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT, DVS132S_CONFIG_EXTINPUT_DETECT_PULSE_POLARITY,
		dvConfigNodeGetBool(node, "DetectPulsePolarity"));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT, DVS132S_CONFIG_EXTINPUT_DETECT_PULSE_LENGTH,
		U32T(dvConfigNodeGetInt(node, "DetectPulseLength")));
	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT, DVS132S_CONFIG_EXTINPUT_RUN_DETECTOR,
		dvConfigNodeGetBool(node, "RunDetector"));

	if (devInfo->extInputHasGenerator) {
		caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
			DVS132S_CONFIG_EXTINPUT_GENERATE_PULSE_POLARITY, dvConfigNodeGetBool(node, "GeneratePulsePolarity"));
		caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
			DVS132S_CONFIG_EXTINPUT_GENERATE_PULSE_INTERVAL, U32T(dvConfigNodeGetInt(node, "GeneratePulseInterval")));
		caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
			DVS132S_CONFIG_EXTINPUT_GENERATE_PULSE_LENGTH, U32T(dvConfigNodeGetInt(node, "GeneratePulseLength")));
		caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
			DVS132S_CONFIG_EXTINPUT_GENERATE_INJECT_ON_RISING_EDGE,
			dvConfigNodeGetBool(node, "GenerateInjectOnRisingEdge"));
		caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
			DVS132S_CONFIG_EXTINPUT_GENERATE_INJECT_ON_FALLING_EDGE,
			dvConfigNodeGetBool(node, "GenerateInjectOnFallingEdge"));
		caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT, DVS132S_CONFIG_EXTINPUT_RUN_GENERATOR,
			dvConfigNodeGetBool(node, "RunGenerator"));
	}
}

static void extInputConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	dvModuleData moduleData = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "DetectRisingEdges")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
				DVS132S_CONFIG_EXTINPUT_DETECT_RISING_EDGES, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "DetectFallingEdges")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
				DVS132S_CONFIG_EXTINPUT_DETECT_FALLING_EDGES, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "DetectPulses")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT, DVS132S_CONFIG_EXTINPUT_DETECT_PULSES,
				changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "DetectPulsePolarity")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
				DVS132S_CONFIG_EXTINPUT_DETECT_PULSE_POLARITY, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "DetectPulseLength")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
				DVS132S_CONFIG_EXTINPUT_DETECT_PULSE_LENGTH, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "RunDetector")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT, DVS132S_CONFIG_EXTINPUT_RUN_DETECTOR,
				changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "GeneratePulsePolarity")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
				DVS132S_CONFIG_EXTINPUT_GENERATE_PULSE_POLARITY, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "GeneratePulseInterval")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
				DVS132S_CONFIG_EXTINPUT_GENERATE_PULSE_INTERVAL, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "GeneratePulseLength")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
				DVS132S_CONFIG_EXTINPUT_GENERATE_PULSE_LENGTH, U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "GenerateInjectOnRisingEdge")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
				DVS132S_CONFIG_EXTINPUT_GENERATE_INJECT_ON_RISING_EDGE, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "GenerateInjectOnFallingEdge")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT,
				DVS132S_CONFIG_EXTINPUT_GENERATE_INJECT_ON_FALLING_EDGE, changeValue.boolean);
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "RunGenerator")) {
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_EXTINPUT, DVS132S_CONFIG_EXTINPUT_RUN_GENERATOR,
				changeValue.boolean);
		}
	}
}

static void usbConfigSend(dvConfigNode node, dvModuleData moduleData) {
	caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_USB, CAER_HOST_CONFIG_USB_BUFFER_NUMBER,
		U32T(dvConfigNodeGetInt(node, "BufferNumber")));
	caerDeviceConfigSet(moduleData->moduleState, CAER_HOST_CONFIG_USB, CAER_HOST_CONFIG_USB_BUFFER_SIZE,
		U32T(dvConfigNodeGetInt(node, "BufferSize")));

	caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_USB, DVS132S_CONFIG_USB_EARLY_PACKET_DELAY,
		U32T(dvConfigNodeGetInt(node, "EarlyPacketDelay")));
	caerDeviceConfigSet(
		moduleData->moduleState, DVS132S_CONFIG_USB, DVS132S_CONFIG_USB_RUN, dvConfigNodeGetBool(node, "Run"));
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
			caerDeviceConfigSet(moduleData->moduleState, DVS132S_CONFIG_USB, DVS132S_CONFIG_USB_EARLY_PACKET_DELAY,
				U32T(changeValue.iint));
		}
		else if (changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "Run")) {
			caerDeviceConfigSet(
				moduleData->moduleState, DVS132S_CONFIG_USB, DVS132S_CONFIG_USB_RUN, changeValue.boolean);
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

static union dvConfigAttributeValue statisticsUpdater(
	void *userData, const char *key, enum dvConfigAttributeType type) {
	UNUSED_ARGUMENT(type); // We know all statistics are always LONG.

	caerDeviceHandle handle = userData;

	union dvConfigAttributeValue statisticValue = {.ilong = 0};

	if (caerStrEquals(key, "muxDroppedDVS")) {
		caerDeviceConfigGet64(
			handle, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_STATISTICS_DVS_DROPPED, (uint64_t *) &statisticValue.ilong);
	}
	else if (caerStrEquals(key, "muxDroppedExtInput")) {
		caerDeviceConfigGet64(handle, DVS132S_CONFIG_MUX, DVS132S_CONFIG_MUX_STATISTICS_EXTINPUT_DROPPED,
			(uint64_t *) &statisticValue.ilong);
	}
	else if (caerStrEquals(key, "dvsTransactionsSuccess")) {
		caerDeviceConfigGet64(handle, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_STATISTICS_TRANSACTIONS_SUCCESS,
			(uint64_t *) &statisticValue.ilong);
	}
	else if (caerStrEquals(key, "dvsTransactionsSkipped")) {
		caerDeviceConfigGet64(handle, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_STATISTICS_TRANSACTIONS_SKIPPED,
			(uint64_t *) &statisticValue.ilong);
	}
	else if (caerStrEquals(key, "dvsTransactionsAll")) {
		uint64_t success = 0;
		caerDeviceConfigGet64(handle, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_STATISTICS_TRANSACTIONS_SUCCESS, &success);

		uint64_t skipped = 0;
		caerDeviceConfigGet64(handle, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_STATISTICS_TRANSACTIONS_SKIPPED, &skipped);

		statisticValue.ilong = I64T(success + skipped);
	}
	else if (caerStrEquals(key, "dvsTransactionsErrored")) {
		uint32_t statisticValue32 = 0;
		caerDeviceConfigGet(
			handle, DVS132S_CONFIG_DVS, DVS132S_CONFIG_DVS_STATISTICS_TRANSACTIONS_ERRORED, &statisticValue32);
		statisticValue.ilong = statisticValue32;
	}

	return (statisticValue);
}
