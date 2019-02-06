#include "statistics.h"

#include "dv-sdk/mainloop.h"

static void statisticsModuleConfigInit(dvConfigNode moduleNode);
static bool statisticsModuleInit(caerModuleData moduleData);
static void statisticsModuleRun(caerModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out);
static void statisticsModuleReset(caerModuleData moduleData, int16_t resetCallSourceID);

static const struct caer_module_functions StatisticsFunctions = {
	.moduleConfigInit = &statisticsModuleConfigInit,
	.moduleInit       = &statisticsModuleInit,
	.moduleRun        = &statisticsModuleRun,
	.moduleConfig     = NULL,
	.moduleExit       = NULL,
	.moduleReset      = &statisticsModuleReset,
};

static const struct caer_event_stream_in StatisticsInputs[] = {{
	.type     = -1,
	.number   = 1,
	.readOnly = true,
}};

static const struct caer_module_info StatisticsInfo = {
	.version           = 1,
	.name              = "Statistics",
	.description       = "Display statistics on events.",
	.type              = DV_MODULE_OUTPUT,
	.memSize           = sizeof(struct caer_statistics_state),
	.functions         = &StatisticsFunctions,
	.inputStreams      = StatisticsInputs,
	.inputStreamsSize  = CAER_EVENT_STREAM_IN_SIZE(StatisticsInputs),
	.outputStreams     = NULL,
	.outputStreamsSize = 0,
};

caerModuleInfo caerModuleGetInfo(void) {
	return (&StatisticsInfo);
}

static void statisticsModuleConfigInit(dvConfigNode moduleNode) {
	dvConfigNodeCreateLong(moduleNode, "divisionFactor", 1000, 1, INT64_MAX, DVCFG_FLAGS_NORMAL,
		"Division factor for statistics display, to get Kilo/Mega/... events shown.");

	dvConfigNodeCreateLong(moduleNode, "eventsTotal", 0, 0, INT64_MAX, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT,
		"Number of events per second.");

	dvConfigNodeCreateLong(moduleNode, "eventsValid", 0, 0, INT64_MAX, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT,
		"Number of valid events per second.");

	dvConfigNodeCreateLong(moduleNode, "packetTSDiff", 0, 0, INT64_MAX, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT,
		"Maximum time difference (in µs) between consecutive packets.");
}

static bool statisticsModuleInit(caerModuleData moduleData) {
	caerStatisticsState state = moduleData->moduleState;

	caerStatisticsInit(state);

	// Configurable division factor.
	state->divisionFactor = U64T(dvConfigNodeGetLong(moduleData->moduleNode, "divisionFactor"));

	return (true);
}

static void statisticsModuleRun(caerModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out) {
	UNUSED_ARGUMENT(out);

	// Interpret variable arguments (same as above in main function).
	caerEventPacketHeaderConst packetHeader = caerEventPacketContainerGetEventPacketConst(in, 0);

	caerStatisticsState state = moduleData->moduleState;

	if (caerStatisticsUpdate(packetHeader, state)) {
		dvConfigNodeUpdateReadOnlyAttribute(moduleData->moduleNode, "eventsTotal", DVCFG_TYPE_LONG,
			(union dvConfigAttributeValue){.ilong = state->currStatsEventsTotal});
		dvConfigNodeUpdateReadOnlyAttribute(moduleData->moduleNode, "eventsValid", DVCFG_TYPE_LONG,
			(union dvConfigAttributeValue){.ilong = state->currStatsEventsValid});
		dvConfigNodeUpdateReadOnlyAttribute(moduleData->moduleNode, "packetTSDiff", DVCFG_TYPE_LONG,
			(union dvConfigAttributeValue){.ilong = state->currStatsPacketTSDiff});
	}
}

static void statisticsModuleReset(caerModuleData moduleData, int16_t resetCallSourceID) {
	UNUSED_ARGUMENT(resetCallSourceID);

	caerStatisticsState state = moduleData->moduleState;

	caerStatisticsReset(state);
}
