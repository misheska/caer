/*
 * Public header for support library.
 * Modules can use this and link to it.
 */

#ifndef DV_SDK_MODULE_H_
#define DV_SDK_MODULE_H_

#include "events/types.hpp"

#include "utils.h"

#ifdef __cplusplus

#	include <atomic>
using atomic_bool          = std::atomic_bool;
using atomic_uint_fast8_t  = std::atomic_uint_fast8_t;
using atomic_uint_fast32_t = std::atomic_uint_fast32_t;
using atomic_int_fast16_t  = std::atomic_int_fast16_t;

#else

#	include <stdatomic.h>

#endif

#ifdef __cplusplus
extern "C" {
#endif

// Module connectivity.
void dvModuleRegisterOutput(const char *name, const char *typeName);
void dvModuleRegisterInput(const char *name, const char *typeName);

struct dvTypedArray *dvModuleOutputAllocate(const char *name, size_t elements);
bool dvModuleOutputCommit(const char *name);

const struct dvTypedArray *dvModuleInputGet(const char *name);
void dvModuleInputRefInc(const char *name, const struct dvTypedArray *data);
void dvModuleInputRefDec(const char *name, const struct dvTypedArray *data);

// Module-related definitions.
enum dvModuleStatus {
	DV_MODULE_STOPPED = 0,
	DV_MODULE_RUNNING = 1,
};

/**
 * Input modules strictly create data, as such they have no input event
 * streams and at least 1 output event stream.
 * Output modules consume data, without modifying it, so they have at
 * least 1 input event stream, and no output event streams. They must
 * set the 'readOnly' flag to true on all their input event streams.
 * Processor modules do something with data, filtering it or creating
 * new data out of it, as such they must have at least 1 input event
 * stream, and at least 1 output event stream (implicit or explicit).
 * Explicit output streams in this case are new data that is declared
 * as output event stream explicitly, while implicit are input streams
 * with their 'readOnly' flag set to false, meaning the data is modified.
 * Output streams can either be undefined and later be determined at
 * runtime, or be well defined. Only one output stream per type is allowed.
 */
enum dvModuleType {
	DV_MODULE_INPUT     = 0,
	DV_MODULE_OUTPUT    = 1,
	DV_MODULE_PROCESSOR = 2,
};

static inline const char *dvModuleTypeToString(enum dvModuleType type) {
	switch (type) {
		case DV_MODULE_INPUT:
			return ("INPUT");
			break;

		case DV_MODULE_OUTPUT:
			return ("OUTPUT");
			break;

		case DV_MODULE_PROCESSOR:
			return ("PROCESSOR");
			break;

		default:
			return ("UNKNOWN");
			break;
	}
}

struct caer_event_stream_in {
	int16_t type;   // Use -1 for any type.
	int16_t number; // Use -1 for any number of.
	bool readOnly;  // True if input is never modified.
};

typedef struct caer_event_stream_in const *caerEventStreamIn;

#define CAER_EVENT_STREAM_IN_SIZE(x) (sizeof(x) / sizeof(struct caer_event_stream_in))

struct caer_event_stream_out {
	int16_t type; // Use -1 for undefined output (determined at runtime from configuration).
};

typedef struct caer_event_stream_out const *caerEventStreamOut;

#define CAER_EVENT_STREAM_OUT_SIZE(x) (sizeof(x) / sizeof(struct caer_event_stream_out))

struct dvModuleDataS {
	int16_t moduleID;
	dvConfigNode moduleNode;
	enum dvModuleStatus moduleStatus;
	atomic_bool running;
	atomic_uint_fast8_t moduleLogLevel;
	atomic_uint_fast32_t configUpdate;
	void *moduleState;
	char *moduleSubSystemString;
};

typedef struct dvModuleDataS *dvModuleData;

struct dvModuleFunctionsS {
	void (*const moduleConfigInit)(dvConfigNode moduleNode); // Can be NULL.
	bool (*const moduleInit)(dvModuleData moduleData);       // Can be NULL.
	void (*const moduleRun)(dvModuleData moduleData, caerEventPacketContainer in, caerEventPacketContainer *out);
	void (*const moduleConfig)(dvModuleData moduleData); // Can be NULL.
	void (*const moduleExit)(dvModuleData moduleData);   // Can be NULL.
};

typedef struct dvModuleFunctionsS const *dvModuleFunctions;

struct dvModuleInfoS {
	uint32_t version;
	const char *description;
	enum dvModuleType type;
	size_t memSize;
	dvModuleFunctions functions;
	size_t inputStreamsSize;
	caerEventStreamIn inputStreams;
	size_t outputStreamsSize;
	caerEventStreamOut outputStreams;
};

typedef struct dvModuleInfoS const *dvModuleInfo;

// Function to be implemented by modules:
dvModuleInfo dvModuleGetInfo(void);

// Functions available to call:
void dvModuleLog(dvModuleData moduleData, enum caer_log_level logLevel, const char *format, ...) ATTRIBUTE_FORMAT(3);
bool dvModuleSetLogString(dvModuleData moduleData, const char *subSystemString);
void dvModuleDefaultConfigListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

#ifdef __cplusplus
}
#endif

#endif /* DV_SDK_MODULE_H_ */
