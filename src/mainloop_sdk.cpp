#include "mainloop.h"

#include <boost/format.hpp>

static dv::MainData *glMainDataPtr;

void dv::MainSDKLibInit(MainData *setMainDataPtr) {
	glMainDataPtr = setMainDataPtr;
}
