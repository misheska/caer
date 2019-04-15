#ifndef AEDAT4_CONVERT_H
#define AEDAT4_CONVERT_H

#include <libcaer/events/common.h>

#include "dv-sdk/module.h"

void dvConvertToAedat4(caerEventPacketHeaderConst oldPacket, dvModuleData moduleData);

#endif // AEDAT4_CONVERT_H
