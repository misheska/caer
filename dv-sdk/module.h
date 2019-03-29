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

struct dvModuleDataS {
	dvConfigNode moduleNode;
	void *moduleState;
};

typedef struct dvModuleDataS *dvModuleData;

struct dvModuleFunctionsS {
	void (*const moduleStaticInit)(
		dvModuleData moduleData); // Can be NULL. ModuleState is always NULL, do not dereference/use.
	bool (*const moduleInit)(dvModuleData moduleData);   // Can be NULL.
	void (*const moduleRun)(dvModuleData moduleData);    // Must be defined.
	void (*const moduleConfig)(dvModuleData moduleData); // Can be NULL.
	void (*const moduleExit)(dvModuleData moduleData);   // Can be NULL.
};

typedef struct dvModuleFunctionsS const *dvModuleFunctions;

struct dvModuleInfoS {
	// Module version (informative).
	int32_t version;
	// Module description (informative).
	const char *description;
	// Size in bytes of module state.
	size_t memSize;
	// Functions to execute to run module.
	dvModuleFunctions functions;
};

typedef struct dvModuleInfoS const *dvModuleInfo;

/**
 * Function to be implemented by modules.
 * Must return a dvModuleInfoS structure pointer,
 * with all the information from your module.
 */
dvModuleInfo dvModuleGetInfo(void);

// Functions available for use: module connectivity.
void dvModuleRegisterType(dvModuleData moduleData, const struct dvType type);
void dvModuleRegisterOutput(dvModuleData moduleData, const char *name, const char *typeName);
void dvModuleRegisterInput(dvModuleData moduleData, const char *name, const char *typeName, bool optional);

struct dvTypedObject *dvModuleOutputAllocate(dvModuleData moduleData, const char *name);
void dvModuleOutputCommit(dvModuleData moduleData, const char *name);

const struct dvTypedObject *dvModuleInputGet(dvModuleData moduleData, const char *name);
void dvModuleInputDismiss(dvModuleData moduleData, const char *name, const struct dvTypedObject *data);

#ifdef __cplusplus
}
#endif

#endif /* DV_SDK_MODULE_H_ */
