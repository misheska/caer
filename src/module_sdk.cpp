#include "dv-sdk/module.h"

bool dvModuleRegisterType(dvConfigNode moduleNode, const struct dvType type) {
}

void dvModuleRegisterOutput(dvConfigNode moduleNode, const char *name, const char *typeName) {
}

void dvModuleRegisterInput(dvConfigNode moduleNode, const char *name, const char *typeName, bool optional) {
}

dvTypedObjectPtr dvModuleOutputAllocate(dvModuleData moduleData, const char *name) {
}

void dvModuleOutputCommit(dvModuleData moduleData, const char *name) {
}

dvTypedObjectConstPtr dvModuleInputGet(dvModuleData moduleData, const char *name) {
}

void dvModuleInputDismiss(dvModuleData moduleData, const char *name, dvTypedObjectConstPtr data) {
}
