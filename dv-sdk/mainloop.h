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

void dvMainloopDataNotifyIncrease(void *p);
void dvMainloopDataNotifyDecrease(void *p);

bool dvMainloopModuleExists(int16_t id);
enum dvModuleType dvMainloopModuleGetType(int16_t id);
uint32_t dvMainloopModuleGetVersion(int16_t id);
dvConfigNode dvMainloopModuleGetConfigNode(int16_t id);
size_t dvMainloopModuleGetInputDeps(int16_t id, int16_t **inputDepIds);
size_t dvMainloopModuleGetOutputRevDeps(int16_t id, int16_t **outputRevDepIds);
dvConfigNode dvMainloopModuleGetSourceNodeForInput(int16_t id, size_t inputNum);
dvConfigNode dvMainloopModuleGetSourceInfoForInput(int16_t id, size_t inputNum);

dvConfigNode dvMainloopGetSourceNode(int16_t sourceID); // Can be NULL.
void *dvMainloopGetSourceState(int16_t sourceID);       // Can be NULL.
dvConfigNode dvMainloopGetSourceInfo(int16_t sourceID); // Can be NULL.

#ifdef __cplusplus
}
#endif

#endif /* DV_SDK_MAINLOOP_H_ */
