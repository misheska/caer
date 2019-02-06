#ifndef SERVICE_H_
#define SERVICE_H_

#include "dv-sdk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

void caerServiceInit(void (*runner)(void));

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_H_ */
