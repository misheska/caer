#include <libcaer/events/polarity.h>

#include <libcaer/filters/dvs_noise.h>

#include "dv-sdk/mainloop.h"

static void caerDVSNoiseFilterConfigInit(dvConfigNode moduleNode);
static bool caerDVSNoiseFilterInit(dvModuleData moduleData);
static void caerDVSNoiseFilterRun(
	dvModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out);
static void caerDVSNoiseFilterConfig(dvModuleData moduleData);
static void caerDVSNoiseFilterExit(dvModuleData moduleData);
static void caerDVSNoiseFilterReset(dvModuleData moduleData, int16_t resetCallSourceID);

static union dvConfigAttributeValue updateHotPixelFiltered(
	void *userData, const char *key, enum dvConfigAttributeType type);
static union dvConfigAttributeValue updateBackgroundActivityFiltered(
	void *userData, const char *key, enum dvConfigAttributeType type);
static union dvConfigAttributeValue updateRefractoryPeriodFiltered(
	void *userData, const char *key, enum dvConfigAttributeType type);
static void caerDVSNoiseFilterConfigCustom(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

static const struct dvModuleFunctionsS DVSNoiseFilterFunctions = {.moduleConfigInit = &caerDVSNoiseFilterConfigInit,
	.moduleInit                                                                        = &caerDVSNoiseFilterInit,
	.moduleRun                                                                         = &caerDVSNoiseFilterRun,
	.moduleConfig                                                                      = &caerDVSNoiseFilterConfig,
	.moduleExit                                                                        = &caerDVSNoiseFilterExit,
	.moduleReset                                                                       = &caerDVSNoiseFilterReset};

static const struct caer_event_stream_in DVSNoiseFilterInputs[]
	= {{.type = POLARITY_EVENT, .number = 1, .readOnly = false}};

static const struct dvModuleInfoS DVSNoiseFilterInfo = {
	.version           = 1,
	.name              = "DVSNoiseFilter",
	.description       = "Filters out noise from DVS change events.",
	.type              = DV_MODULE_PROCESSOR,
	.memSize           = 0,
	.functions         = &DVSNoiseFilterFunctions,
	.inputStreams      = DVSNoiseFilterInputs,
	.inputStreamsSize  = CAER_EVENT_STREAM_IN_SIZE(DVSNoiseFilterInputs),
	.outputStreams     = NULL,
	.outputStreamsSize = 0,
};

dvModuleInfo caerModuleGetInfo(void) {
	return (&DVSNoiseFilterInfo);
}

static void caerDVSNoiseFilterConfigInit(dvConfigNode moduleNode) {
	dvConfigNodeCreateBool(moduleNode, "hotPixelLearn", false, DVCFG_FLAGS_NORMAL,
		"Learn the position of current hot (abnormally active) pixels, so they can be filtered out.");
    dvConfigNodeAttributeModifierButton(moduleNode, "hotPixelLearn", "EXECUTE");
	dvConfigNodeCreateInt(moduleNode, "hotPixelTime", 1000000, 0, 30000000, DVCFG_FLAGS_NORMAL,
		"Time in µs to accumulate events for learning new hot pixels.");
	dvConfigNodeCreateInt(moduleNode, "hotPixelCount", 10000, 0, 10000000, DVCFG_FLAGS_NORMAL,
		"Number of events needed in a learning time period for a pixel to be considered hot.");

	dvConfigNodeCreateBool(moduleNode, "hotPixelEnable", false, DVCFG_FLAGS_NORMAL, "Enable the hot pixel filter.");
	dvConfigNodeCreateLong(moduleNode, "hotPixelFiltered", 0, 0, INT64_MAX, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT,
		"Number of events filtered out by the hot pixel filter.");

	dvConfigNodeCreateBool(
		moduleNode, "backgroundActivityEnable", true, DVCFG_FLAGS_NORMAL, "Enable the background activity filter.");
	dvConfigNodeCreateBool(moduleNode, "backgroundActivityTwoLevels", false, DVCFG_FLAGS_NORMAL,
		"Use two-level background activity filtering.");
	dvConfigNodeCreateBool(moduleNode, "backgroundActivityCheckPolarity", false, DVCFG_FLAGS_NORMAL,
		"Consider polarity when filtering background activity.");
	dvConfigNodeCreateInt(moduleNode, "backgroundActivitySupportMin", 1, 1, 8, DVCFG_FLAGS_NORMAL,
		"Minimum number of direct neighbor pixels that must support this pixel for it to be valid.");
	dvConfigNodeCreateInt(moduleNode, "backgroundActivitySupportMax", 8, 1, 8, DVCFG_FLAGS_NORMAL,
		"Maximum number of direct neighbor pixels that can support this pixel for it to be valid.");
	dvConfigNodeCreateInt(moduleNode, "backgroundActivityTime", 2000, 0, 10000000, DVCFG_FLAGS_NORMAL,
		"Maximum time difference in µs for events to be considered correlated and not be filtered out.");
	dvConfigNodeCreateLong(moduleNode, "backgroundActivityFiltered", 0, 0, INT64_MAX,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT,
		"Number of events filtered out by the background activity filter.");

	dvConfigNodeCreateBool(
		moduleNode, "refractoryPeriodEnable", true, DVCFG_FLAGS_NORMAL, "Enable the refractory period filter.");
	dvConfigNodeCreateInt(moduleNode, "refractoryPeriodTime", 100, 0, 10000000, DVCFG_FLAGS_NORMAL,
		"Minimum time between events to not be filtered out.");
	dvConfigNodeCreateLong(moduleNode, "refractoryPeriodFiltered", 0, 0, INT64_MAX,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT, "Number of events filtered out by the refractory period filter.");
}

static union dvConfigAttributeValue updateHotPixelFiltered(
	void *userData, const char *key, enum dvConfigAttributeType type) {
	UNUSED_ARGUMENT(key);
	UNUSED_ARGUMENT(type);

	caerFilterDVSNoise state                  = userData;
	union dvConfigAttributeValue statisticValue = {.ilong = 0};

	caerFilterDVSNoiseConfigGet(state, CAER_FILTER_DVS_HOTPIXEL_STATISTICS, (uint64_t *) &statisticValue.ilong);

	return (statisticValue);
}

static union dvConfigAttributeValue updateBackgroundActivityFiltered(
	void *userData, const char *key, enum dvConfigAttributeType type) {
	UNUSED_ARGUMENT(key);
	UNUSED_ARGUMENT(type);

	caerFilterDVSNoise state                  = userData;
	union dvConfigAttributeValue statisticValue = {.ilong = 0};

	caerFilterDVSNoiseConfigGet(
		state, CAER_FILTER_DVS_BACKGROUND_ACTIVITY_STATISTICS, (uint64_t *) &statisticValue.ilong);

	return (statisticValue);
}

static union dvConfigAttributeValue updateRefractoryPeriodFiltered(
	void *userData, const char *key, enum dvConfigAttributeType type) {
	UNUSED_ARGUMENT(key);
	UNUSED_ARGUMENT(type);

	caerFilterDVSNoise state                  = userData;
	union dvConfigAttributeValue statisticValue = {.ilong = 0};

	caerFilterDVSNoiseConfigGet(
		state, CAER_FILTER_DVS_REFRACTORY_PERIOD_STATISTICS, (uint64_t *) &statisticValue.ilong);

	return (statisticValue);
}

static bool caerDVSNoiseFilterInit(dvModuleData moduleData) {
	// Wait for input to be ready. All inputs, once they are up and running, will
	// have a valid sourceInfo node to query, especially if dealing with data.
	// Allocate map using info from sourceInfo.
	dvConfigNode sourceInfo = caerMainloopModuleGetSourceInfoForInput(moduleData->moduleID, 0);
	if (sourceInfo == NULL) {
		return (false);
	}

	int32_t sizeX = dvConfigNodeGetInt(sourceInfo, "polaritySizeX");
	int32_t sizeY = dvConfigNodeGetInt(sourceInfo, "polaritySizeY");

	moduleData->moduleState = caerFilterDVSNoiseInitialize(U16T(sizeX), U16T(sizeY));
	if (moduleData->moduleState == NULL) {
		caerModuleLog(moduleData, CAER_LOG_ERROR, "Failed to initialize DVS Noise filter.");
		return (false);
	}

	caerDVSNoiseFilterConfig(moduleData);

	caerFilterDVSNoiseConfigSet(
		moduleData->moduleState, CAER_FILTER_DVS_LOG_LEVEL, atomic_load(&moduleData->moduleLogLevel));

	dvConfigNodeAttributeUpdaterAdd(
		moduleData->moduleNode, "hotPixelFiltered", DVCFG_TYPE_LONG, &updateHotPixelFiltered, moduleData->moduleState);
	dvConfigNodeAttributeUpdaterAdd(moduleData->moduleNode, "backgroundActivityFiltered", DVCFG_TYPE_LONG,
		&updateBackgroundActivityFiltered, moduleData->moduleState);
	dvConfigNodeAttributeUpdaterAdd(moduleData->moduleNode, "refractoryPeriodFiltered", DVCFG_TYPE_LONG,
		&updateRefractoryPeriodFiltered, moduleData->moduleState);

	// Add config listeners last, to avoid having them dangling if Init doesn't succeed.
	dvConfigNodeAddAttributeListener(moduleData->moduleNode, moduleData, &caerModuleConfigDefaultListener);
	dvConfigNodeAddAttributeListener(moduleData->moduleNode, moduleData->moduleState, &caerDVSNoiseFilterConfigCustom);

	// Nothing that can fail here.
	return (true);
}

static void caerDVSNoiseFilterRun(
	dvModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out) {
	UNUSED_ARGUMENT(out);

	caerPolarityEventPacket polarity
		= (caerPolarityEventPacket) caerEventPacketContainerFindEventPacketByType(in, POLARITY_EVENT);

	caerFilterDVSNoiseApply(moduleData->moduleState, polarity);
}

static void caerDVSNoiseFilterConfig(dvModuleData moduleData) {
	caerFilterDVSNoise state = moduleData->moduleState;

	caerFilterDVSNoiseConfigSet(
		state, CAER_FILTER_DVS_HOTPIXEL_TIME, U32T(dvConfigNodeGetInt(moduleData->moduleNode, "hotPixelTime")));
	caerFilterDVSNoiseConfigSet(
		state, CAER_FILTER_DVS_HOTPIXEL_COUNT, U32T(dvConfigNodeGetInt(moduleData->moduleNode, "hotPixelCount")));

	caerFilterDVSNoiseConfigSet(
		state, CAER_FILTER_DVS_HOTPIXEL_ENABLE, dvConfigNodeGetBool(moduleData->moduleNode, "hotPixelEnable"));

	caerFilterDVSNoiseConfigSet(state, CAER_FILTER_DVS_BACKGROUND_ACTIVITY_ENABLE,
		dvConfigNodeGetBool(moduleData->moduleNode, "backgroundActivityEnable"));
	caerFilterDVSNoiseConfigSet(state, CAER_FILTER_DVS_BACKGROUND_ACTIVITY_TWO_LEVELS,
		dvConfigNodeGetBool(moduleData->moduleNode, "backgroundActivityTwoLevels"));
	caerFilterDVSNoiseConfigSet(state, CAER_FILTER_DVS_BACKGROUND_ACTIVITY_CHECK_POLARITY,
		dvConfigNodeGetBool(moduleData->moduleNode, "backgroundActivityCheckPolarity"));
	caerFilterDVSNoiseConfigSet(state, CAER_FILTER_DVS_BACKGROUND_ACTIVITY_SUPPORT_MIN,
		U8T(dvConfigNodeGetInt(moduleData->moduleNode, "backgroundActivitySupportMin")));
	caerFilterDVSNoiseConfigSet(state, CAER_FILTER_DVS_BACKGROUND_ACTIVITY_SUPPORT_MAX,
		U8T(dvConfigNodeGetInt(moduleData->moduleNode, "backgroundActivitySupportMax")));
	caerFilterDVSNoiseConfigSet(state, CAER_FILTER_DVS_BACKGROUND_ACTIVITY_TIME,
		U32T(dvConfigNodeGetInt(moduleData->moduleNode, "backgroundActivityTime")));

	caerFilterDVSNoiseConfigSet(state, CAER_FILTER_DVS_REFRACTORY_PERIOD_ENABLE,
		dvConfigNodeGetBool(moduleData->moduleNode, "refractoryPeriodEnable"));
	caerFilterDVSNoiseConfigSet(state, CAER_FILTER_DVS_REFRACTORY_PERIOD_TIME,
		U32T(dvConfigNodeGetInt(moduleData->moduleNode, "refractoryPeriodTime")));

	caerFilterDVSNoiseConfigSet(
		state, CAER_FILTER_DVS_LOG_LEVEL, U8T(dvConfigNodeGetInt(moduleData->moduleNode, "logLevel")));
}

static void caerDVSNoiseFilterConfigCustom(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);
	UNUSED_ARGUMENT(changeValue);

	caerFilterDVSNoise state = userData;

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "hotPixelLearn")
		&& changeValue.boolean) {
		// Button-like configuration parameters need special
		// handling as only the change is delivered, so we have to listen for
		// it directly. The usual Config mechanism doesn't work, as Get()
		// would always return false.
		caerFilterDVSNoiseConfigSet(state, CAER_FILTER_DVS_HOTPIXEL_LEARN, true);
	}
}

static void caerDVSNoiseFilterExit(dvModuleData moduleData) {
	// Remove listener, which can reference invalid memory in userData.
	dvConfigNodeRemoveAttributeListener(moduleData->moduleNode, moduleData, &caerModuleConfigDefaultListener);
	dvConfigNodeRemoveAttributeListener(moduleData->moduleNode, moduleData->moduleState, &caerDVSNoiseFilterConfigCustom);

	dvConfigNodeAttributeUpdaterRemoveAll(moduleData->moduleNode);

	caerFilterDVSNoiseDestroy(moduleData->moduleState);
}

static void caerDVSNoiseFilterReset(dvModuleData moduleData, int16_t resetCallSourceID) {
	UNUSED_ARGUMENT(resetCallSourceID);

	caerFilterDVSNoiseConfigSet(moduleData->moduleState, CAER_FILTER_DVS_RESET, true);
}
