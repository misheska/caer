/*
 * Public header for support library.
 * Modules can use this and link to it.
 */

#ifndef DV_SDK_MODULE_H_
#define DV_SDK_MODULE_H_

#include "events/types.hpp"

#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Input modules strictly create data, as such they have no input event
 * streams and at least 1 output event stream.
 * Output modules consume data, without modifying it, so they have at
 * least 1 input event stream, and no output event streams.
 * Processor modules do something with data, filtering it or creating
 * new data out of it, as such they must have at least 1 input event
 * stream, and at least 1 output event stream.
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

struct dvModuleDataS {
	dvConfigNode moduleNode;
	void *moduleState;
};

typedef struct dvModuleDataS *dvModuleData;

struct dvModuleFunctionsS {
	void (*const moduleStaticInit)(dvConfigNode moduleNode); // Can be NULL.
	bool (*const moduleInit)(dvModuleData moduleData);       // Can be NULL.
	void (*const moduleRun)(dvModuleData moduleData);        // Must be defined.
	void (*const moduleConfig)(dvModuleData moduleData);     // Can be NULL.
	void (*const moduleExit)(dvModuleData moduleData);       // Can be NULL.
};

typedef struct dvModuleFunctionsS const *dvModuleFunctions;

struct dvModuleInfoS {
	uint32_t version;
	const char *description;
	enum dvModuleType type;
	size_t memSize;
	dvModuleFunctions functions;
};

typedef struct dvModuleInfoS const *dvModuleInfo;

// Function to be implemented by modules:
dvModuleInfo dvModuleGetInfo(void);

// Functions available for use: module connectivity.
bool dvModuleRegisterType(dvConfigNode moduleNode, const struct dvType type);
void dvModuleRegisterOutput(dvConfigNode moduleNode, const char *name, const char *typeName);
void dvModuleRegisterInput(dvConfigNode moduleNode, const char *name, const char *typeName, bool optional);

dvTypedObjectPtr dvModuleOutputAllocate(dvModuleData moduleData, const char *name);
bool dvModuleOutputCommit(dvModuleData moduleData, const char *name);

dvTypedObjectConstPtr dvModuleInputGet(dvModuleData moduleData, const char *name);
void dvModuleInputRefInc(dvModuleData moduleData, const char *name, dvTypedObjectConstPtr data);
void dvModuleInputRefDec(dvModuleData moduleData, const char *name, dvTypedObjectConstPtr data);

#ifdef __cplusplus
}
#endif

#endif /* DV_SDK_MODULE_H_ */
