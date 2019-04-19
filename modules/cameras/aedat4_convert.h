#ifndef AEDAT4_CONVERT_H
#define AEDAT4_CONVERT_H

#include <libcaer/events/common.h>

#include "dv-sdk/module.h"

#ifdef __cplusplus
extern "C" {
#endif

void dvConvertToAedat4(caerEventPacketHeaderConst oldPacket, dvModuleData moduleData);

#ifdef __cplusplus
}
#endif

#endif // AEDAT4_CONVERT_H
