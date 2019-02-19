#ifndef DVCONFIG_H_
#define DVCONFIG_H_

#ifdef __cplusplus

#	include <cerrno>
#	include <cinttypes>
#	include <cstdint>
#	include <cstdio>
#	include <cstdlib>

#else

#	include <errno.h>
#	include <inttypes.h>
#	include <stdbool.h>
#	include <stdint.h>
#	include <stdio.h>
#	include <stdlib.h>

#endif

#ifdef __cplusplus
extern "C" {
#endif

// dv::Config Node
typedef struct dv_config_node *dvConfigNode;

enum dvConfigAttributeType {
	DVCFG_TYPE_UNKNOWN = -1,
	DVCFG_TYPE_BOOL    = 0,
	DVCFG_TYPE_INT     = 1,
	DVCFG_TYPE_LONG    = 2,
	DVCFG_TYPE_FLOAT   = 3,
	DVCFG_TYPE_DOUBLE  = 4,
	DVCFG_TYPE_STRING  = 5,
};

union dvConfigAttributeValue {
	bool boolean;
	int32_t iint;
	int64_t ilong;
	float ffloat;
	double ddouble;
	char *string;
};

union dvConfigAttributeRange {
	int32_t intRange;
	int64_t longRange;
	float floatRange;
	double doubleRange;
	int32_t stringRange;
};

struct dvConfigAttributeRanges {
	union dvConfigAttributeRange min;
	union dvConfigAttributeRange max;
};

enum dvConfigAttributeFlags {
	DVCFG_FLAGS_NORMAL    = 0,
	DVCFG_FLAGS_READ_ONLY = 1,
	DVCFG_FLAGS_NO_EXPORT = 2,
};

enum dvConfigNodeEvents {
	DVCFG_NODE_CHILD_ADDED   = 0,
	DVCFG_NODE_CHILD_REMOVED = 1,
};

enum dvConfigAttributeEvents {
	DVCFG_ATTRIBUTE_ADDED           = 0,
	DVCFG_ATTRIBUTE_MODIFIED        = 1,
	DVCFG_ATTRIBUTE_REMOVED         = 2,
	DVCFG_ATTRIBUTE_MODIFIED_CREATE = 3,
};

typedef void (*dvConfigNodeChangeListener)(
	dvConfigNode node, void *userData, enum dvConfigNodeEvents event, const char *changeNode);

typedef void (*dvConfigAttributeChangeListener)(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

const char *dvConfigNodeGetName(dvConfigNode node);
const char *dvConfigNodeGetPath(dvConfigNode node);

/**
 * This returns a reference to a node, and as such must be carefully mediated with
 * any dvConfigNodeRemoveNode() calls.
 */
dvConfigNode dvConfigNodeGetParent(dvConfigNode node);
/**
 * Remember to free the resulting array. This returns references to nodes,
 * and as such must be carefully mediated with any dvConfigNodeRemoveNode() calls.
 */
dvConfigNode *dvConfigNodeGetChildren(dvConfigNode node, size_t *numChildren); // Walk all children.

void dvConfigNodeAddNodeListener(dvConfigNode node, void *userData, dvConfigNodeChangeListener node_changed);
void dvConfigNodeRemoveNodeListener(dvConfigNode node, void *userData, dvConfigNodeChangeListener node_changed);
void dvConfigNodeRemoveAllNodeListeners(dvConfigNode node);

void dvConfigNodeAddAttributeListener(
	dvConfigNode node, void *userData, dvConfigAttributeChangeListener attribute_changed);
void dvConfigNodeRemoveAttributeListener(
	dvConfigNode node, void *userData, dvConfigAttributeChangeListener attribute_changed);
void dvConfigNodeRemoveAllAttributeListeners(dvConfigNode node);

/**
 * Careful, only use if no references exist to this node and all its children.
 * References are created by dvConfigTreeGetNode(), dvConfigNodeGetRelativeNode(),
 * dvConfigNodeGetParent() and dvConfigNodeGetChildren().
 */
void dvConfigNodeRemoveNode(dvConfigNode node);
/**
 * Careful, only use if no references exist to this node's children.
 * References are created by dvConfigTreeGetNode(), dvConfigNodeGetRelativeNode(),
 * dvConfigNodeGetParent() and dvConfigNodeGetChildren().
 */
void dvConfigNodeRemoveSubTree(dvConfigNode node);
void dvConfigNodeClearSubTree(dvConfigNode startNode, bool clearStartNode);

void dvConfigNodeCreateAttribute(dvConfigNode node, const char *key, enum dvConfigAttributeType type,
	union dvConfigAttributeValue defaultValue, const struct dvConfigAttributeRanges ranges, int flags,
	const char *description);
void dvConfigNodeRemoveAttribute(dvConfigNode node, const char *key, enum dvConfigAttributeType type);
void dvConfigNodeRemoveAllAttributes(dvConfigNode node);
bool dvConfigNodeExistsAttribute(dvConfigNode node, const char *key, enum dvConfigAttributeType type);
bool dvConfigNodePutAttribute(
	dvConfigNode node, const char *key, enum dvConfigAttributeType type, union dvConfigAttributeValue value);
union dvConfigAttributeValue dvConfigNodeGetAttribute(
	dvConfigNode node, const char *key, enum dvConfigAttributeType type);
bool dvConfigNodeUpdateReadOnlyAttribute(
	dvConfigNode node, const char *key, enum dvConfigAttributeType type, union dvConfigAttributeValue value);

void dvConfigNodeCreateBool(dvConfigNode node, const char *key, bool defaultValue, int flags, const char *description);
bool dvConfigNodePutBool(dvConfigNode node, const char *key, bool value);
bool dvConfigNodeGetBool(dvConfigNode node, const char *key);
void dvConfigNodeCreateInt(dvConfigNode node, const char *key, int32_t defaultValue, int32_t minValue, int32_t maxValue,
	int flags, const char *description);
bool dvConfigNodePutInt(dvConfigNode node, const char *key, int32_t value);
int32_t dvConfigNodeGetInt(dvConfigNode node, const char *key);
void dvConfigNodeCreateLong(dvConfigNode node, const char *key, int64_t defaultValue, int64_t minValue,
	int64_t maxValue, int flags, const char *description);
bool dvConfigNodePutLong(dvConfigNode node, const char *key, int64_t value);
int64_t dvConfigNodeGetLong(dvConfigNode node, const char *key);
void dvConfigNodeCreateFloat(dvConfigNode node, const char *key, float defaultValue, float minValue, float maxValue,
	int flags, const char *description);
bool dvConfigNodePutFloat(dvConfigNode node, const char *key, float value);
float dvConfigNodeGetFloat(dvConfigNode node, const char *key);
void dvConfigNodeCreateDouble(dvConfigNode node, const char *key, double defaultValue, double minValue, double maxValue,
	int flags, const char *description);
bool dvConfigNodePutDouble(dvConfigNode node, const char *key, double value);
double dvConfigNodeGetDouble(dvConfigNode node, const char *key);
void dvConfigNodeCreateString(dvConfigNode node, const char *key, const char *defaultValue, int32_t minLength,
	int32_t maxLength, int flags, const char *description);
bool dvConfigNodePutString(dvConfigNode node, const char *key, const char *value);
char *dvConfigNodeGetString(dvConfigNode node, const char *key);

bool dvConfigNodeExportNodeToXML(dvConfigNode node, int fd);
bool dvConfigNodeExportSubTreeToXML(dvConfigNode node, int fd);
bool dvConfigNodeImportNodeFromXML(dvConfigNode node, int fd, bool strict);
bool dvConfigNodeImportSubTreeFromXML(dvConfigNode node, int fd, bool strict);

bool dvConfigNodeStringToAttributeConverter(dvConfigNode node, const char *key, const char *type, const char *value);
const char **dvConfigNodeGetChildNames(dvConfigNode node, size_t *numNames);
const char **dvConfigNodeGetAttributeKeys(dvConfigNode node, size_t *numKeys);
enum dvConfigAttributeType dvConfigNodeGetAttributeType(dvConfigNode node, const char *key);
struct dvConfigAttributeRanges dvConfigNodeGetAttributeRanges(
	dvConfigNode node, const char *key, enum dvConfigAttributeType type);
int dvConfigNodeGetAttributeFlags(dvConfigNode node, const char *key, enum dvConfigAttributeType type);
char *dvConfigNodeGetAttributeDescription(dvConfigNode node, const char *key, enum dvConfigAttributeType type);

void dvConfigNodeAttributeModifierButton(dvConfigNode node, const char *key, const char *type);
void dvConfigNodeAttributeModifierListOptions(
	dvConfigNode node, const char *key, const char *listOptions, bool allowMultipleSelections);
void dvConfigNodeAttributeModifierFileChooser(dvConfigNode node, const char *key, const char *typeAndExtensions);
void dvConfigNodeAttributeModifierUnit(dvConfigNode node, const char *key, const char *unitInformation);
void dvConfigNodeAttributeModifierPriorityAttributes(dvConfigNode node, const char *priorityAttributes);

bool dvConfigNodeExistsRelativeNode(dvConfigNode node, const char *nodePath);
/**
 * This returns a reference to a node, and as such must be carefully mediated with
 * any dvConfigNodeRemoveNode() calls.
 */
dvConfigNode dvConfigNodeGetRelativeNode(dvConfigNode node, const char *nodePath);

// dv::Config Helper functions
const char *dvConfigHelperTypeToStringConverter(enum dvConfigAttributeType type);
enum dvConfigAttributeType dvConfigHelperStringToTypeConverter(const char *typeString);
char *dvConfigHelperValueToStringConverter(enum dvConfigAttributeType type, union dvConfigAttributeValue value);
union dvConfigAttributeValue dvConfigHelperStringToValueConverter(
	enum dvConfigAttributeType type, const char *valueString);
char *dvConfigHelperFlagsToStringConverter(int flags);
int dvConfigHelperStringToFlagsConverter(const char *flagsString);
char *dvConfigHelperRangesToStringConverter(enum dvConfigAttributeType type, struct dvConfigAttributeRanges ranges);
struct dvConfigAttributeRanges dvConfigHelperStringToRangesConverter(
	enum dvConfigAttributeType type, const char *rangesString);

// dv::Config Tree
typedef struct dv_config_tree *dvConfigTree;
typedef void (*dvConfigTreeErrorLogCallback)(const char *msg, bool fatal);

dvConfigTree dvConfigTreeGlobal(void);
dvConfigTree dvConfigTreeNew(void);
void dvConfigTreeErrorLogCallbackSet(dvConfigTreeErrorLogCallback error_log_cb);
dvConfigTreeErrorLogCallback dvConfigTreeErrorLogCallbackGet(void);

bool dvConfigTreeExistsNode(dvConfigTree st, const char *nodePath);
/**
 * This returns a reference to a node, and as such must be carefully mediated with
 * any dvConfigNodeRemoveNode() calls.
 */
dvConfigNode dvConfigTreeGetNode(dvConfigTree st, const char *nodePath);

typedef union dvConfigAttributeValue (*dvConfigAttributeUpdater)(
	void *userData, const char *key, enum dvConfigAttributeType type);

void dvConfigNodeAttributeUpdaterAdd(dvConfigNode node, const char *key, enum dvConfigAttributeType type,
	dvConfigAttributeUpdater updater, void *updaterUserData);
void dvConfigNodeAttributeUpdaterRemove(dvConfigNode node, const char *key, enum dvConfigAttributeType type,
	dvConfigAttributeUpdater updater, void *updaterUserData);
void dvConfigNodeAttributeUpdaterRemoveAll(dvConfigNode node);
void dvConfigTreeAttributeUpdaterRemoveAll(dvConfigTree tree);
bool dvConfigTreeAttributeUpdaterRun(dvConfigTree tree);

/**
 * Listener must be able to deal with userData being NULL at any moment.
 * This can happen due to concurrent changes from this setter.
 */
void dvConfigTreeGlobalNodeListenerSet(dvConfigTree tree, dvConfigNodeChangeListener node_changed, void *userData);
/**
 * Listener must be able to deal with userData being NULL at any moment.
 * This can happen due to concurrent changes from this setter.
 */
void dvConfigTreeGlobalAttributeListenerSet(
	dvConfigTree tree, dvConfigAttributeChangeListener attribute_changed, void *userData);

#ifdef __cplusplus
}
#endif

#endif /* DVCONFIG_H_ */
