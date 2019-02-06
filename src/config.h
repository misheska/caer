#ifndef CONFIG_H_
#define CONFIG_H_

#include "dv-sdk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DV_CONFIG_FILE_NAME ".dv-config.xml"

// Create configuration storage, initialize it with content from the
// configuration file, and apply eventual CLI overrides.
void caerConfigInit(int argc, char *argv[]);
void caerConfigWriteBack(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H_ */
