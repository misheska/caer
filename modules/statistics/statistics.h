#ifndef STATISTICS_H_
#define STATISTICS_H_

#include "caer-sdk/utils.h"

#include <libcaer/events/common.h>
#include "caer-sdk/cross/portable_time.h"
#include <sys/time.h>
#include <time.h>

#define CAER_STATISTICS_STRING_EVT_TOTAL "Total events/second: %10" PRIu64
#define CAER_STATISTICS_STRING_EVT_VALID "Valid events/second: %10" PRIu64
#define CAER_STATISTICS_STRING_PKT_TSDIFF "Max packets time diff (µs): %10" PRIu64

struct caer_statistics_state {
	uint64_t divisionFactor;
	char *currentStatisticsStringTotal;
	char *currentStatisticsStringValid;
	char *currentStatisticsStringTSDiff;
	// Internal book-keeping.
	struct timespec lastTime;
	uint64_t totalEventsCounter;
	uint64_t validEventsCounter;
	int64_t packetTimeDifference;
	int64_t packetLastTimestamp;
};

typedef struct caer_statistics_state *caerStatisticsState;

// For reuse inside other modules.
static inline bool caerStatisticsStringInit(caerStatisticsState state) {
	// Total and Valid parts have same length. TSDiff is bigger, so use that one.
	size_t maxSplitStatStringLength = (size_t) snprintf(NULL, 0, CAER_STATISTICS_STRING_PKT_TSDIFF, UINT64_MAX);

	state->currentStatisticsStringTotal
		= (char *) calloc(maxSplitStatStringLength + 1, sizeof(char)); // +1 for NUL termination.
	if (state->currentStatisticsStringTotal == NULL) {
		return (false);
	}

	state->currentStatisticsStringValid
		= (char *) calloc(maxSplitStatStringLength + 1, sizeof(char)); // +1 for NUL termination.
	if (state->currentStatisticsStringValid == NULL) {
		free(state->currentStatisticsStringTotal);
		state->currentStatisticsStringTotal = NULL;

		return (false);
	}

	state->currentStatisticsStringTSDiff
		= (char *) calloc(maxSplitStatStringLength + 1, sizeof(char)); // +1 for NUL termination.
	if (state->currentStatisticsStringTSDiff == NULL) {
		free(state->currentStatisticsStringTotal);
		state->currentStatisticsStringTotal = NULL;

		free(state->currentStatisticsStringValid);
		state->currentStatisticsStringValid = NULL;

		return (false);
	}

	// Initialize to current time.
	portable_clock_gettime_monotonic(&state->lastTime);

	// Set division factor to 1 by default (avoid division by zero).
	state->divisionFactor = 1;

	return (true);
}

static inline void caerStatisticsStringUpdate(caerEventPacketHeaderConst packetHeader, caerStatisticsState state) {
	// Only non-NULL packets (with content!) contribute to the event count.
	if (packetHeader != NULL) {
		int32_t eventNumber = caerEventPacketHeaderGetEventNumber(packetHeader);

		if (eventNumber > 0) {
			state->totalEventsCounter += U64T(eventNumber);
			state->validEventsCounter += U64T(caerEventPacketHeaderGetEventValid(packetHeader));

			const void *firstEvent = caerGenericEventGetEvent(packetHeader, 0);
			int64_t currTimestamp  = caerGenericEventGetTimestamp64(firstEvent, packetHeader);

			int64_t currDifference = currTimestamp - state->packetLastTimestamp;
			if (currDifference > state->packetTimeDifference) {
				state->packetTimeDifference = currDifference;
			}

			const void *lastEvent      = caerGenericEventGetEvent(packetHeader, eventNumber - 1);
			state->packetLastTimestamp = caerGenericEventGetTimestamp64(lastEvent, packetHeader);
		}
	}

	// Print up-to-date statistic roughly every second, taking into account possible deviations.
	struct timespec currentTime;
	portable_clock_gettime_monotonic(&currentTime);

	uint64_t diffNanoTime = (uint64_t)(((int64_t)(currentTime.tv_sec - state->lastTime.tv_sec) * 1000000000LL)
									   + (int64_t)(currentTime.tv_nsec - state->lastTime.tv_nsec));

	// DiffNanoTime is the difference in nanoseconds; we want to trigger roughly every second.
	if (diffNanoTime >= 1000000000LLU) {
		// Print current values.
		uint64_t totalEventsPerTime
			= (state->totalEventsCounter * (1000000000LLU / state->divisionFactor)) / diffNanoTime;
		uint64_t validEventsPerTime
			= (state->validEventsCounter * (1000000000LLU / state->divisionFactor)) / diffNanoTime;

		sprintf(state->currentStatisticsStringTotal, CAER_STATISTICS_STRING_EVT_TOTAL, totalEventsPerTime);
		sprintf(state->currentStatisticsStringValid, CAER_STATISTICS_STRING_EVT_VALID, validEventsPerTime);
		sprintf(
			state->currentStatisticsStringTSDiff, CAER_STATISTICS_STRING_PKT_TSDIFF, U64T(state->packetTimeDifference));

		// Reset for next update.
		state->totalEventsCounter   = 0;
		state->validEventsCounter   = 0;
		state->packetTimeDifference = 0;
		state->lastTime             = currentTime;
	}
}

static inline void caerStatisticsStringExit(caerStatisticsState state) {
	// Reclaim string memory.
	if (state->currentStatisticsStringTotal != NULL) {
		free(state->currentStatisticsStringTotal);
		state->currentStatisticsStringTotal = NULL;
	}

	if (state->currentStatisticsStringValid != NULL) {
		free(state->currentStatisticsStringValid);
		state->currentStatisticsStringValid = NULL;
	}

	if (state->currentStatisticsStringTSDiff != NULL) {
		free(state->currentStatisticsStringTSDiff);
		state->currentStatisticsStringTSDiff = NULL;
	}
}

static inline void caerStatisticsStringReset(caerStatisticsState state) {
	// Reset counters.
	state->totalEventsCounter   = 0;
	state->validEventsCounter   = 0;
	state->packetTimeDifference = 0;
	state->packetLastTimestamp  = 0;

	// Update to current time.
	portable_clock_gettime_monotonic(&state->lastTime);
}

#endif /* STATISTICS_H_ */
