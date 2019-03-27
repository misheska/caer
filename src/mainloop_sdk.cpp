#include "mainloop.h"

#include <boost/format.hpp>

static dv::MainData *glMainDataPtr;

void dv::MainSDKLibInit(MainData *setMainDataPtr) {
	glMainDataPtr = setMainDataPtr;
}

const struct dvType dvTypeSystemGetInfoByIdentifier(const char *tIdentifier) {
	return (glMainDataPtr->typeSystem.getTypeInfo(tIdentifier, nullptr));
}

const struct dvType dvTypeSystemGetInfoByID(uint32_t tId) {
	return (glMainDataPtr->typeSystem.getTypeInfo(tId, nullptr));
}
