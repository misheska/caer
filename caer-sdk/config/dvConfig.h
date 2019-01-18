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
	DVCFG_TYPE_INT     = 3,
	DVCFG_TYPE_LONG    = 4,
	DVCFG_TYPE_FLOAT   = 5,
	DVCFG_TYPE_DOUBLE  = 6,
	DVCFG_TYPE_STRING  = 7,
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
	int32_t iintRange;
	int64_t ilongRange;
	float ffloatRange;
	double ddoubleRange;
	size_t stringRange;
};

struct dvConfigAttributeRanges {
	union dvConfigAttributeRange min;
	union dvConfigAttributeRange max;
};

enum dvConfigAttributeFlags {
	DVCFG_FLAGS_NORMAL      = 0,
	DVCFG_FLAGS_READ_ONLY   = 1,
	DVCFG_FLAGS_NOTIFY_ONLY = 2,
	DVCFG_FLAGS_NO_EXPORT   = 4,
};

enum dvConfigNodeEvents {
	DVCFG_NODE_CHILD_ADDED   = 0,
	DVCFG_NODE_CHILD_REMOVED = 1,
};

enum dvConfigAttributeEvents {
	DVCFG_ATTRIBUTE_ADDED    = 0,
	DVCFG_ATTRIBUTE_MODIFIED = 1,
	DVCFG_ATTRIBUTE_REMOVED  = 2,
};

typedef void (*dvConfigNodeChangeListener)(
	dvConfigNode node, void *userData, enum dvConfigNodeEvents event, const char *changeNode);

typedef void (*dvConfigAttributeChangeListener)(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

const char *sshsNodeGetName(dvConfigNode node);
const char *sshsNodeGetPath(dvConfigNode node);

/**
 * This returns a reference to a node, and as such must be carefully mediated with
 * any sshsNodeRemoveNode() calls.
 */
dvConfigNode sshsNodeGetParent(dvConfigNode node);
/**
 * Remember to free the resulting array. This returns references to nodes,
 * and as such must be carefully mediated with any sshsNodeRemoveNode() calls.
 */
dvConfigNode *sshsNodeGetChildren(dvConfigNode node, size_t *numChildren); // Walk all children.

void sshsNodeAddNodeListener(dvConfigNode node, void *userData, dvConfigNodeChangeListener node_changed);
void sshsNodeRemoveNodeListener(dvConfigNode node, void *userData, dvConfigNodeChangeListener node_changed);
void sshsNodeRemoveAllNodeListeners(dvConfigNode node);

void sshsNodeAddAttributeListener(dvConfigNode node, void *userData, dvConfigAttributeChangeListener attribute_changed);
void sshsNodeRemoveAttributeListener(
	dvConfigNode node, void *userData, dvConfigAttributeChangeListener attribute_changed);
void sshsNodeRemoveAllAttributeListeners(dvConfigNode node);

/**
 * Careful, only use if no references exist to this node and all its children.
 * References are created by sshsGetNode(), sshsGetRelativeNode(),
 * sshsNodeGetParent() and sshsNodeGetChildren().
 */
void sshsNodeRemoveNode(dvConfigNode node);
void sshsNodeClearSubTree(dvConfigNode startNode, bool clearStartNode);

void sshsNodeCreateAttribute(dvConfigNode node, const char *key, enum dvConfigAttributeType type,
	union dvConfigAttributeValue defaultValue, const struct dvConfigAttributeRanges ranges, int flags,
	const char *description);
void sshsNodeRemoveAttribute(dvConfigNode node, const char *key, enum dvConfigAttributeType type);
void sshsNodeRemoveAllAttributes(dvConfigNode node);
bool sshsNodeAttributeExists(dvConfigNode node, const char *key, enum dvConfigAttributeType type);
bool sshsNodePutAttribute(
	dvConfigNode node, const char *key, enum dvConfigAttributeType type, union dvConfigAttributeValue value);
union dvConfigAttributeValue sshsNodeGetAttribute(dvConfigNode node, const char *key, enum dvConfigAttributeType type);
bool sshsNodeUpdateReadOnlyAttribute(
	dvConfigNode node, const char *key, enum dvConfigAttributeType type, union dvConfigAttributeValue value);

void sshsNodeCreateBool(dvConfigNode node, const char *key, bool defaultValue, int flags, const char *description);
bool sshsNodePutBool(dvConfigNode node, const char *key, bool value);
bool sshsNodeGetBool(dvConfigNode node, const char *key);
void sshsNodeCreateInt(dvConfigNode node, const char *key, int32_t defaultValue, int32_t minValue, int32_t maxValue,
	int flags, const char *description);
bool sshsNodePutInt(dvConfigNode node, const char *key, int32_t value);
int32_t sshsNodeGetInt(dvConfigNode node, const char *key);
void sshsNodeCreateLong(dvConfigNode node, const char *key, int64_t defaultValue, int64_t minValue, int64_t maxValue,
	int flags, const char *description);
bool sshsNodePutLong(dvConfigNode node, const char *key, int64_t value);
int64_t sshsNodeGetLong(dvConfigNode node, const char *key);
void sshsNodeCreateFloat(dvConfigNode node, const char *key, float defaultValue, float minValue, float maxValue,
	int flags, const char *description);
bool sshsNodePutFloat(dvConfigNode node, const char *key, float value);
float sshsNodeGetFloat(dvConfigNode node, const char *key);
void sshsNodeCreateDouble(dvConfigNode node, const char *key, double defaultValue, double minValue, double maxValue,
	int flags, const char *description);
bool sshsNodePutDouble(dvConfigNode node, const char *key, double value);
double sshsNodeGetDouble(dvConfigNode node, const char *key);
void sshsNodeCreateString(dvConfigNode node, const char *key, const char *defaultValue, size_t minLength,
	size_t maxLength, int flags, const char *description);
bool sshsNodePutString(dvConfigNode node, const char *key, const char *value);
char *sshsNodeGetString(dvConfigNode node, const char *key);

bool sshsNodeExportNodeToXML(dvConfigNode node, int fd);
bool sshsNodeExportSubTreeToXML(dvConfigNode node, int fd);
bool sshsNodeImportNodeFromXML(dvConfigNode node, int fd, bool strict);
bool sshsNodeImportSubTreeFromXML(dvConfigNode node, int fd, bool strict);

bool sshsNodeStringToAttributeConverter(dvConfigNode node, const char *key, const char *type, const char *value);
const char **sshsNodeGetChildNames(dvConfigNode node, size_t *numNames);
const char **sshsNodeGetAttributeKeys(dvConfigNode node, size_t *numKeys);
enum dvConfigAttributeType sshsNodeGetAttributeType(dvConfigNode node, const char *key);
struct dvConfigAttributeRanges sshsNodeGetAttributeRanges(
	dvConfigNode node, const char *key, enum dvConfigAttributeType type);
int sshsNodeGetAttributeFlags(dvConfigNode node, const char *key, enum dvConfigAttributeType type);
char *sshsNodeGetAttributeDescription(dvConfigNode node, const char *key, enum dvConfigAttributeType type);

// Helper functions
const char *sshsHelperTypeToStringConverter(enum dvConfigAttributeType type);
enum dvConfigAttributeType sshsHelperStringToTypeConverter(const char *typeString);
char *sshsHelperValueToStringConverter(enum dvConfigAttributeType type, union dvConfigAttributeValue value);
union dvConfigAttributeValue sshsHelperStringToValueConverter(enum dvConfigAttributeType type, const char *valueString);
char *sshsHelperFlagsToStringConverter(int flags);
int sshsHelperStringToFlagsConverter(const char *flagsString);
char *sshsHelperRangesToStringConverter(enum dvConfigAttributeType type, struct dvConfigAttributeRanges ranges);
struct dvConfigAttributeRanges sshsHelperStringToRangesConverter(
	enum dvConfigAttributeType type, const char *rangesString);

void sshsNodeCreateAttributeListOptions(
	dvConfigNode node, const char *key, const char *listOptions, bool allowMultipleSelections);
void sshsNodeCreateAttributeFileChooser(dvConfigNode node, const char *key, const char *allowedExtensions);

// dv::Config Tree
typedef struct dv_config_tree *dvConfigTree;
typedef void (*dvConfigTreeErrorLogCallback)(const char *msg, bool fatal);

dvConfigTree sshsGetGlobal(void);
void sshsSetGlobalErrorLogCallback(dvConfigTreeErrorLogCallback error_log_cb);
dvConfigTreeErrorLogCallback sshsGetGlobalErrorLogCallback(void);
dvConfigTree sshsNew(void);
bool sshsExistsNode(dvConfigTree st, const char *nodePath);
/**
 * This returns a reference to a node, and as such must be carefully mediated with
 * any sshsNodeRemoveNode() calls.
 */
dvConfigNode sshsGetNode(dvConfigTree st, const char *nodePath);
bool sshsExistsRelativeNode(dvConfigNode node, const char *nodePath);
/**
 * This returns a reference to a node, and as such must be carefully mediated with
 * any sshsNodeRemoveNode() calls.
 */
dvConfigNode sshsGetRelativeNode(dvConfigNode node, const char *nodePath);

typedef union dvConfigAttributeValue (*dvConfigAttributeUpdater)(
	void *userData, const char *key, enum dvConfigAttributeType type);

void sshsAttributeUpdaterAdd(dvConfigNode node, const char *key, enum dvConfigAttributeType type,
	dvConfigAttributeUpdater updater, void *updaterUserData);
void sshsAttributeUpdaterRemove(dvConfigNode node, const char *key, enum dvConfigAttributeType type,
	dvConfigAttributeUpdater updater, void *updaterUserData);
void sshsAttributeUpdaterRemoveAllForNode(dvConfigNode node);
void sshsAttributeUpdaterRemoveAll(dvConfigTree tree);
bool sshsAttributeUpdaterRun(dvConfigTree tree);

/**
 * Listener must be able to deal with userData being NULL at any moment.
 * This can happen due to concurrent changes from this setter.
 */
void sshsGlobalNodeListenerSet(dvConfigTree tree, dvConfigNodeChangeListener node_changed, void *userData);
/**
 * Listener must be able to deal with userData being NULL at any moment.
 * This can happen due to concurrent changes from this setter.
 */
void sshsGlobalAttributeListenerSet(
	dvConfigTree tree, dvConfigAttributeChangeListener attribute_changed, void *userData);

#ifdef __cplusplus
}
#endif

#endif /* DVCONFIG_H_ */
