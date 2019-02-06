/*
 * log.h
 *
 *  Created on: Dec 30, 2013
 *      Author: llongi
 */

#ifndef LOG_H_
#define LOG_H_

#include "dv-sdk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DV_LOG_FILE_NAME ".dv-logger.log"

void dvLogInit(void);

#ifdef __cplusplus
}
#endif

#endif /* LOG_H_ */
