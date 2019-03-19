#include "dv-sdk/module.h"

bool dvModuleRegisterType(dvConfigNode moduleNode, const struct dvType type) {
}

void dvModuleRegisterOutput(dvConfigNode moduleNode, const char *name, const char *typeName) {
}

void dvModuleRegisterInput(dvConfigNode moduleNode, const char *name, const char *typeName, bool optional) {
}

dvTypedArrayPtr dvModuleOutputAllocate(dvModuleData moduleData, const char *name, size_t elements) {
}

bool dvModuleOutputCommit(dvModuleData moduleData, const char *name) {
}

dvTypedArrayConstPtr dvModuleInputGet(dvModuleData moduleData, const char *name) {
}

void dvModuleInputRefInc(dvModuleData moduleData, const char *name, dvTypedArrayConstPtr data) {
}

void dvModuleInputRefDec(dvModuleData moduleData, const char *name, dvTypedArrayConstPtr data) {
}
