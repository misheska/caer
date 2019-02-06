/*
 * Public header for support library.
 * Modules can use this and link to it.
 */

#ifndef DV_SDK_MAINLOOP_H_
#define DV_SDK_MAINLOOP_H_

#include "module.h"
#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif

void caerMainloopDataNotifyIncrease(void *p);
void caerMainloopDataNotifyDecrease(void *p);

bool caerMainloopStreamExists(int16_t sourceId, int16_t typeId);

bool caerMainloopModuleExists(int16_t id);
enum dvModuleType caerMainloopModuleGetType(int16_t id);
uint32_t caerMainloopModuleGetVersion(int16_t id);
enum dvModuleStatus caerMainloopModuleGetStatus(int16_t id);
dvConfigNode caerMainloopModuleGetConfigNode(int16_t id);
size_t caerMainloopModuleGetInputDeps(int16_t id, int16_t **inputDepIds);
size_t caerMainloopModuleGetOutputRevDeps(int16_t id, int16_t **outputRevDepIds);
size_t caerMainloopModuleResetOutputRevDeps(int16_t id);
dvConfigNode caerMainloopModuleGetSourceNodeForInput(int16_t id, size_t inputNum);
dvConfigNode caerMainloopModuleGetSourceInfoForInput(int16_t id, size_t inputNum);

dvConfigNode caerMainloopGetSourceNode(int16_t sourceID); // Can be NULL.
void *caerMainloopGetSourceState(int16_t sourceID);       // Can be NULL.
dvConfigNode caerMainloopGetSourceInfo(int16_t sourceID); // Can be NULL.

#ifdef __cplusplus
}
#endif

#endif /* DV_SDK_MAINLOOP_H_ */
