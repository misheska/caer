#include "dv-sdk/module.h"

bool dvModuleRegisterType(dvConfigNode moduleNode, const dvType type) {
}

void dvModuleRegisterOutput(dvConfigNode moduleNode, const char *name, const char *typeName) {
}

void dvModuleRegisterInput(dvConfigNode moduleNode, const char *name, const char *typeName, bool optional) {
}

dvTypedArray *dvModuleOutputAllocate(dvModuleData moduleData, const char *name, size_t elements) {
}

bool dvModuleOutputCommit(dvModuleData moduleData, const char *name) {
}

const dvTypedArray *dvModuleInputGet(dvModuleData moduleData, const char *name) {
}

void dvModuleInputRefInc(dvModuleData moduleData, const char *name, const dvTypedArray *data) {
}

void dvModuleInputRefDec(dvModuleData moduleData, const char *name, const dvTypedArray *data) {
}
