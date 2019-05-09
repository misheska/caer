#ifndef DEVICES_DISCOVERY_HPP_
#define DEVICES_DISCOVERY_HPP_

#include "dv-sdk/utils.h"

namespace dv {

void DevicesUpdateListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event, const char *changeKey,
	enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
void DevicesUpdateList();

} // namespace dv

#endif /* DEVICES_DISCOVERY_HPP_ */
