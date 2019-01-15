#ifndef SSHS_HPP_
#define SSHS_HPP_

#include "sshs.h"

#include <string>
#include <vector>

enum class sshsAttributeType {
	UNKNOWN = SSHS_UNKNOWN,
	BOOL    = SSHS_BOOL,
	INT     = SSHS_INT,
	LONG    = SSHS_LONG,
	FLOAT   = SSHS_FLOAT,
	DOUBLE  = SSHS_DOUBLE,
	STRING  = SSHS_STRING,
};

template<sshsAttributeType T> struct sshsAttributeTypeGenerator {
	static_assert(true, "Only BOOL, INT, LONG, FLOAT, DOUBLE and STRING are supported.");
};

template<> struct sshsAttributeTypeGenerator<sshsAttributeType::BOOL> {
	using type                                                 = bool;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_BOOL;
};

template<> struct sshsAttributeTypeGenerator<sshsAttributeType::INT> {
	using type                                                 = int32_t;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_INT;
};

template<> struct sshsAttributeTypeGenerator<sshsAttributeType::LONG> {
	using type                                                 = int64_t;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_LONG;
};

template<> struct sshsAttributeTypeGenerator<sshsAttributeType::FLOAT> {
	using type                                                 = float;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_FLOAT;
};

template<> struct sshsAttributeTypeGenerator<sshsAttributeType::DOUBLE> {
	using type                                                 = double;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_DOUBLE;
};

template<> struct sshsAttributeTypeGenerator<sshsAttributeType::STRING> {
	using type                                                 = char *;
	using const_type                                           = const char *;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_STRING;
};

template<sshsAttributeType T> struct sshsAttributeRangeGenerator {
	using rangeType = typename sshsAttributeTypeGenerator<T>::type;
};

template<> struct sshsAttributeRangeGenerator<sshsAttributeType::BOOL> { using rangeType = size_t; };

template<> struct sshsAttributeRangeGenerator<sshsAttributeType::STRING> { using rangeType = size_t; };

template<sshsAttributeType T> struct sshsAttributeValue {
public:
	typename sshsAttributeTypeGenerator<T>::type value;

	sshsAttributeValue(typename sshsAttributeTypeGenerator<T>::type v) : value(v) {
	}

	sshsAttributeValue(const sshs_node_attr_value v) {
		switch (T) {
			case sshsAttributeType::BOOL:
				value = v.boolean;
				break;

			case sshsAttributeType::INT:
				value = v.iint;
				break;

			case sshsAttributeType::LONG:
				value = v.ilong;
				break;

			case sshsAttributeType::FLOAT:
				value = v.ffloat;
				break;

			case sshsAttributeType::DOUBLE:
				value = v.ddouble;
				break;

			default:
				static_assert(true, "sshsAttributeValue<STRING> is specialized.");
				break;
		}
	}

	sshs_node_attr_value getCUnion() const {
		sshs_node_attr_value cUnion;

		switch (T) {
			case sshsAttributeType::BOOL:
				cUnion.boolean = value;
				break;

			case sshsAttributeType::INT:
				cUnion.iint = value;
				break;

			case sshsAttributeType::LONG:
				cUnion.ilong = value;
				break;

			case sshsAttributeType::FLOAT:
				cUnion.ffloat = value;
				break;

			case sshsAttributeType::DOUBLE:
				cUnion.ddouble = value;
				break;

			default:
				static_assert(true, "sshsAttributeValue<STRING> is specialized.");
				break;
		}

		return (cUnion);
	}
};

template<> struct sshsAttributeValue<sshsAttributeType::STRING> {
public:
	typename sshsAttributeTypeGenerator<sshsAttributeType::STRING>::const_type value;

	sshsAttributeValue(typename sshsAttributeTypeGenerator<sshsAttributeType::STRING>::const_type v) : value(v) {
	}

	// Specialize for STRING so we can also accept std::string.
	sshsAttributeValue(const std::string &v) : value(v.c_str()) {
	}

	sshsAttributeValue(const sshs_node_attr_value v) {
		value = v.string;
	}

	sshs_node_attr_value getCUnion() const {
		sshs_node_attr_value cUnion;

		cUnion.string = const_cast<char *>(value);

		return (cUnion);
	}
};

template<sshsAttributeType T> struct sshsAttributeRange {
public:
	typename sshsAttributeRangeGenerator<T>::rangeType range;

	sshsAttributeRange(typename sshsAttributeRangeGenerator<T>::rangeType r) : range(r) {
	}
};

template<sshsAttributeType T> struct sshsAttributeRanges {
public:
	sshsAttributeRange<T> min;
	sshsAttributeRange<T> max;

	sshsAttributeRanges(typename sshsAttributeRangeGenerator<T>::rangeType minVal,
		typename sshsAttributeRangeGenerator<T>::rangeType maxVal) :
		min(minVal),
		max(maxVal) {
	}

	sshs_node_attr_ranges getCStruct() const {
		sshs_node_attr_ranges cStruct;

		switch (T) {
			case sshsAttributeType::BOOL:
				static_assert(true, "sshsAttributeRanges<BOOL> is specialized.");
				break;

			case sshsAttributeType::INT:
				cStruct.min.iintRange = min.range;
				cStruct.max.iintRange = max.range;
				break;

			case sshsAttributeType::LONG:
				cStruct.min.ilongRange = min.range;
				cStruct.max.ilongRange = max.range;
				break;

			case sshsAttributeType::FLOAT:
				cStruct.min.ffloatRange = min.range;
				cStruct.max.ffloatRange = max.range;
				break;

			case sshsAttributeType::DOUBLE:
				cStruct.min.ddoubleRange = min.range;
				cStruct.max.ddoubleRange = max.range;
				break;

			case sshsAttributeType::STRING:
				cStruct.min.stringRange = min.range;
				cStruct.max.stringRange = max.range;
				break;
		}

		return (cStruct);
	}
};

template<> struct sshsAttributeRanges<sshsAttributeType::BOOL> {
public:
	sshsAttributeRange<sshsAttributeType::BOOL> min;
	sshsAttributeRange<sshsAttributeType::BOOL> max;

	sshsAttributeRanges() : min(0), max(0) {
	}

	sshs_node_attr_ranges getCStruct() const {
		sshs_node_attr_ranges cStruct;

		cStruct.min.stringRange = 0;
		cStruct.max.stringRange = 0;

		return (cStruct);
	}
};

enum class sshsAttributeFlags {
	NORMAL      = SSHS_FLAGS_NORMAL,
	READ_ONLY   = SSHS_FLAGS_READ_ONLY,
	NOTIFY_ONLY = SSHS_FLAGS_NOTIFY_ONLY,
	NO_EXPORT   = SSHS_FLAGS_NO_EXPORT,
};

inline sshsAttributeFlags operator|(sshsAttributeFlags lhs, sshsAttributeFlags rhs) {
	using T = std::underlying_type_t<sshsAttributeFlags>;
	return (static_cast<sshsAttributeFlags>(static_cast<T>(lhs) | static_cast<T>(rhs)));
}

inline sshsAttributeFlags &operator|=(sshsAttributeFlags &lhs, sshsAttributeFlags rhs) {
	lhs = lhs | rhs; // Call | overload above.
	return (lhs);
}

inline int getCFlags(sshsAttributeFlags f) {
	return (static_cast<std::underlying_type_t<sshsAttributeFlags>>(f));
}

class sshsn {
private:
	sshsNode node;

public:
	sshsn(sshsNode n) : node(n) {
	}

	std::string getName() {
		return (sshsNodeGetName(node));
	}

	std::string getPath() {
		return (sshsNodeGetPath(node));
	}

	/**
	 * This returns a reference to a node, and as such must be carefully mediated with
	 * any sshsNodeRemoveNode() calls.
	 */
	sshsn getParent() {
		return (sshsNodeGetParent(node));
	}

	/**
	 * This returns a reference to a node, and as such must be carefully mediated with
	 * any sshsNodeRemoveNode() calls.
	 */
	std::vector<sshsn> getChildren() {
		std::vector<sshsn> children;

		size_t numChildren      = 0;
		sshsNode *childrenArray = sshsNodeGetChildren(node, &numChildren);

		if (numChildren > 0) {
			children.reserve(numChildren);

			for (size_t i = 0; i < numChildren; i++) {
				children.emplace_back(childrenArray[i]);
			}

			free(childrenArray);
		}

		return (children);
	}

	void addNodeListener(void *userData, sshsNodeChangeListener node_changed) {
		sshsNodeAddNodeListener(node, userData, node_changed);
	}

	void removeNodeListener(void *userData, sshsNodeChangeListener node_changed) {
		sshsNodeRemoveNodeListener(node, userData, node_changed);
	}

	void removeAllNodeListeners() {
		sshsNodeRemoveAllNodeListeners(node);
	}

	void addAttributeListener(void *userData, sshsAttributeChangeListener attribute_changed) {
		sshsNodeAddAttributeListener(node, userData, attribute_changed);
	}

	void removeAttributeListener(void *userData, sshsAttributeChangeListener attribute_changed) {
		sshsNodeRemoveAttributeListener(node, userData, attribute_changed);
	}

	void removeAllAttributeListeners() {
		sshsNodeRemoveAllAttributeListeners(node);
	}

	/**
	 * Careful, only use if no references exist to this node and all its children.
	 * References are created by sshsg::getNode(), sshsg::getRelativeNode(),
	 * getParent() and getChildren().
	 */
	void removeNode() {
		sshsNodeRemoveNode(node);
	}

	void clearSubTree(bool clearThisNode) {
		sshsNodeClearSubTree(node, clearThisNode);
	}

	template<sshsAttributeType T>
	void createAttribute(const std::string &key, const sshsAttributeValue<T> defaultValue,
		const sshsAttributeRanges<T> ranges, const sshsAttributeFlags flags, const std::string &description) {
		sshsNodeCreateAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType,
			defaultValue.getCUnion(), ranges.getCStruct(), getCFlags(flags), description.c_str());
	}

	template<sshsAttributeType T> void removeAttribute(const std::string &key) {
		sshsNodeRemoveAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType);
	}

	void removeAllAttributes() {
		sshsNodeRemoveAllAttributes(node);
	}

	template<sshsAttributeType T> bool existsAttribute(const std::string &key) {
		return (sshsNodeAttributeExists(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType));
	}

	template<sshsAttributeType T> bool putAttribute(const std::string &key, const sshsAttributeValue<T> value) {
		return (
			sshsNodePutAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType, value.getCUnion()));
	}

	template<sshsAttributeType T> sshsAttributeValue<T> getAttribute(const std::string &key) {
		return (sshsAttributeValue<T>(
			sshsNodeGetAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType)));
	}

	template<sshsAttributeType T>
	bool updateReadOnlyAttribute(const std::string &key, const sshsAttributeValue<T> value) {
		return (sshsNodeUpdateReadOnlyAttribute(
			node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType, value.getCUnion()));
	}
};

class sshsc {
	sshsc() {
		sshsn(nullptr).createAttribute<sshsAttributeType::STRING>("baa", "default",
			sshsAttributeRanges<sshsAttributeType::STRING>(10, 20),
			sshsAttributeFlags::NORMAL | sshsAttributeFlags::NO_EXPORT, "raa");
	}

	void sshsNodeCreateBool(sshsNode node, const char *key, bool defaultValue, int flags, const char *description);
	bool sshsNodePutBool(sshsNode node, const char *key, bool value);
	bool sshsNodeGetBool(sshsNode node, const char *key);
	void sshsNodeCreateInt(sshsNode node, const char *key, int32_t defaultValue, int32_t minValue, int32_t maxValue,
		int flags, const char *description);
	bool sshsNodePutInt(sshsNode node, const char *key, int32_t value);
	int32_t sshsNodeGetInt(sshsNode node, const char *key);
	void sshsNodeCreateLong(sshsNode node, const char *key, int64_t defaultValue, int64_t minValue, int64_t maxValue,
		int flags, const char *description);
	bool sshsNodePutLong(sshsNode node, const char *key, int64_t value);
	int64_t sshsNodeGetLong(sshsNode node, const char *key);
	void sshsNodeCreateFloat(sshsNode node, const char *key, float defaultValue, float minValue, float maxValue,
		int flags, const char *description);
	bool sshsNodePutFloat(sshsNode node, const char *key, float value);
	float sshsNodeGetFloat(sshsNode node, const char *key);
	void sshsNodeCreateDouble(sshsNode node, const char *key, double defaultValue, double minValue, double maxValue,
		int flags, const char *description);
	bool sshsNodePutDouble(sshsNode node, const char *key, double value);
	double sshsNodeGetDouble(sshsNode node, const char *key);
	void sshsNodeCreateString(sshsNode node, const char *key, const char *defaultValue, size_t minLength,
		size_t maxLength, int flags, const char *description);
	bool sshsNodePutString(sshsNode node, const char *key, const char *value);
	char *sshsNodeGetString(sshsNode node, const char *key);

	bool sshsNodeExportNodeToXML(sshsNode node, int fd);
	bool sshsNodeExportSubTreeToXML(sshsNode node, int fd);
	bool sshsNodeImportNodeFromXML(sshsNode node, int fd, bool strict);
	bool sshsNodeImportSubTreeFromXML(sshsNode node, int fd, bool strict);

	bool sshsNodeStringToAttributeConverter(sshsNode node, const char *key, const char *type, const char *value);
	const char **sshsNodeGetChildNames(sshsNode node, size_t *numNames);
	const char **sshsNodeGetAttributeKeys(sshsNode node, size_t *numKeys);
	enum sshs_node_attr_value_type sshsNodeGetAttributeType(sshsNode node, const char *key);
	struct sshs_node_attr_ranges sshsNodeGetAttributeRanges(
		sshsNode node, const char *key, enum sshs_node_attr_value_type type);
	int sshsNodeGetAttributeFlags(sshsNode node, const char *key, enum sshs_node_attr_value_type type);
	char *sshsNodeGetAttributeDescription(sshsNode node, const char *key, enum sshs_node_attr_value_type type);

	// Helper functions
	const char *sshsHelperTypeToStringConverter(enum sshs_node_attr_value_type type);
	enum sshs_node_attr_value_type sshsHelperStringToTypeConverter(const char *typeString);
	char *sshsHelperValueToStringConverter(enum sshs_node_attr_value_type type, union sshs_node_attr_value value);
	union sshs_node_attr_value sshsHelperStringToValueConverter(
		enum sshs_node_attr_value_type type, const char *valueString);
	char *sshsHelperFlagsToStringConverter(int flags);
	int sshsHelperStringToFlagsConverter(const char *flagsString);
	char *sshsHelperRangesToStringConverter(enum sshs_node_attr_value_type type, struct sshs_node_attr_ranges ranges);
	struct sshs_node_attr_ranges sshsHelperStringToRangesConverter(
		enum sshs_node_attr_value_type type, const char *rangesString);

	void sshsNodeCreateAttributeListOptions(
		sshsNode node, const char *key, const char *listOptions, bool allowMultipleSelections);
	void sshsNodeCreateAttributeFileChooser(sshsNode node, const char *key, const char *allowedExtensions);

	// SSHS
	typedef struct sshs_struct *sshs;
	typedef void (*sshsErrorLogCallback)(const char *msg, bool fatal);

	sshs sshsGetGlobal(void);
	void sshsSetGlobalErrorLogCallback(sshsErrorLogCallback error_log_cb);
	sshsErrorLogCallback sshsGetGlobalErrorLogCallback(void);
	sshs sshsNew(void);
	bool sshsExistsNode(sshs st, const char *nodePath);
	/**
	 * This returns a reference to a node, and as such must be carefully mediated with
	 * any sshsNodeRemoveNode() calls.
	 */
	sshsNode sshsGetNode(sshs st, const char *nodePath);
	bool sshsExistsRelativeNode(sshsNode node, const char *nodePath);
	/**
	 * This returns a reference to a node, and as such must be carefully mediated with
	 * any sshsNodeRemoveNode() calls.
	 */
	sshsNode sshsGetRelativeNode(sshsNode node, const char *nodePath);

	typedef union sshs_node_attr_value (*sshsAttributeUpdater)(
		void *userData, const char *key, enum sshs_node_attr_value_type type);

	void sshsAttributeUpdaterAdd(sshsNode node, const char *key, enum sshs_node_attr_value_type type,
		sshsAttributeUpdater updater, void *updaterUserData);
	void sshsAttributeUpdaterRemove(sshsNode node, const char *key, enum sshs_node_attr_value_type type,
		sshsAttributeUpdater updater, void *updaterUserData);
	void sshsAttributeUpdaterRemoveAllForNode(sshsNode node);
	void sshsAttributeUpdaterRemoveAll(sshs tree);
	bool sshsAttributeUpdaterRun(sshs tree);

	/**
	 * Listener must be able to deal with userData being NULL at any moment.
	 * This can happen due to concurrent changes from this setter.
	 */
	void sshsGlobalNodeListenerSet(sshs tree, sshsNodeChangeListener node_changed, void *userData);
	/**
	 * Listener must be able to deal with userData being NULL at any moment.
	 * This can happen due to concurrent changes from this setter.
	 */
	void sshsGlobalAttributeListenerSet(sshs tree, sshsAttributeChangeListener attribute_changed, void *userData);
};

inline void sshsNodeCreate(
	sshsNode node, const std::string &key, bool defaultValue, int flags, const std::string &description) {
	sshsNodeCreateBool(node, key.c_str(), defaultValue, flags, description.c_str());
}

inline void sshsNodeCreate(sshsNode node, const std::string &key, int32_t defaultValue, int32_t minValue,
	int32_t maxValue, int flags, const std::string &description) {
	sshsNodeCreateInt(node, key.c_str(), defaultValue, minValue, maxValue, flags, description.c_str());
}

inline void sshsNodeCreate(sshsNode node, const std::string &key, int64_t defaultValue, int64_t minValue,
	int64_t maxValue, int flags, const std::string &description) {
	sshsNodeCreateLong(node, key.c_str(), defaultValue, minValue, maxValue, flags, description.c_str());
}

inline void sshsNodeCreate(sshsNode node, const std::string &key, float defaultValue, float minValue, float maxValue,
	int flags, const std::string &description) {
	sshsNodeCreateFloat(node, key.c_str(), defaultValue, minValue, maxValue, flags, description.c_str());
}

inline void sshsNodeCreate(sshsNode node, const std::string &key, double defaultValue, double minValue, double maxValue,
	int flags, const std::string &description) {
	sshsNodeCreateDouble(node, key.c_str(), defaultValue, minValue, maxValue, flags, description.c_str());
}

inline void sshsNodeCreate(sshsNode node, const std::string &key, const char *defaultValue, size_t minLength,
	size_t maxLength, int flags, const std::string &description) {
	sshsNodeCreateString(node, key.c_str(), defaultValue, minLength, maxLength, flags, description.c_str());
}

inline void sshsNodeCreate(sshsNode node, const std::string &key, const std::string &defaultValue, size_t minLength,
	size_t maxLength, int flags, const std::string &description) {
	sshsNodeCreateString(node, key.c_str(), defaultValue.c_str(), minLength, maxLength, flags, description.c_str());
}

inline bool sshsNodePut(sshsNode node, const std::string &key, bool value) {
	return (sshsNodePutBool(node, key.c_str(), value));
}

inline bool sshsNodePut(sshsNode node, const std::string &key, int32_t value) {
	return (sshsNodePutInt(node, key.c_str(), value));
}

inline bool sshsNodePut(sshsNode node, const std::string &key, int64_t value) {
	return (sshsNodePutLong(node, key.c_str(), value));
}

inline bool sshsNodePut(sshsNode node, const std::string &key, float value) {
	return (sshsNodePutFloat(node, key.c_str(), value));
}

inline bool sshsNodePut(sshsNode node, const std::string &key, double value) {
	return (sshsNodePutDouble(node, key.c_str(), value));
}

inline bool sshsNodePut(sshsNode node, const std::string &key, const char *value) {
	return (sshsNodePutString(node, key.c_str(), value));
}

inline bool sshsNodePut(sshsNode node, const std::string &key, const std::string &value) {
	return (sshsNodePutString(node, key.c_str(), value.c_str()));
}

// Additional getter for std::string.
inline std::string sshsNodeGetStdString(sshsNode node, const std::string &key) {
	char *str = sshsNodeGetString(node, key.c_str());
	std::string cppStr(str);
	free(str);
	return (cppStr);
}

// Additional updaters for std::string.
inline bool sshsNodeUpdateReadOnlyAttribute(
	sshsNode node, const std::string &key, enum sshs_node_attr_value_type type, union sshs_node_attr_value value) {
	return (sshsNodeUpdateReadOnlyAttribute(node, key.c_str(), type, value));
}

inline bool sshsNodeUpdateReadOnlyAttribute(sshsNode node, const std::string &key, const std::string &value) {
	union sshs_node_attr_value newValue;
	newValue.string = const_cast<char *>(value.c_str());
	return (sshsNodeUpdateReadOnlyAttribute(node, key, SSHS_STRING, newValue));
}

// Additional functions for std::string.
inline void sshsNodeRemoveAttribute(sshsNode node, const std::string &key, enum sshs_node_attr_value_type type) {
	return (sshsNodeRemoveAttribute(node, key.c_str(), type));
}

inline bool sshsNodeAttributeExists(sshsNode node, const std::string &key, enum sshs_node_attr_value_type type) {
	return (sshsNodeAttributeExists(node, key.c_str(), type));
}

inline void sshsNodeCreateAttributeListOptions(
	sshsNode node, const std::string &key, const std::string &listOptions, bool allowMultipleSelections) {
	sshsNodeCreateAttributeListOptions(node, key.c_str(), listOptions.c_str(), allowMultipleSelections);
}

inline void sshsNodeCreateAttributeFileChooser(
	sshsNode node, const std::string &key, const std::string &allowedExtensions) {
	sshsNodeCreateAttributeFileChooser(node, key.c_str(), allowedExtensions.c_str());
}

// std::string variants of node getters.
inline bool sshsExistsNode(sshs st, const std::string &nodePath) {
	return (sshsExistsNode(st, nodePath.c_str()));
}

inline sshsNode sshsGetNode(sshs st, const std::string &nodePath) {
	return (sshsGetNode(st, nodePath.c_str()));
}

inline bool sshsExistsRelativeNode(sshsNode node, const std::string &nodePath) {
	return (sshsExistsRelativeNode(node, nodePath.c_str()));
}

inline sshsNode sshsGetRelativeNode(sshsNode node, const std::string &nodePath) {
	return (sshsGetRelativeNode(node, nodePath.c_str()));
}

#endif /* SSHS_HPP_ */
