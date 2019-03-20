#ifndef CONFIG_HPP_
#define CONFIG_HPP_

#include "dv-sdk/utils.h"

#define DV_CONFIG_FILE_NAME ".dv-config.xml"

namespace dv {

// Create configuration storage, initialize it with content from the
// configuration file, and apply eventual CLI overrides.
void ConfigInit(int argc, char *argv[]);

void ConfigWriteBackListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
void ConfigWriteBack();

} // namespace dv

#endif /* CONFIG_HPP_ */
