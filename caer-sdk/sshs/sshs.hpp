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
	using type                                                 = std::string;
	using const_type                                           = const std::string;
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
				value = static_cast<int32_t>(v.iint);
				break;

			case sshsAttributeType::LONG:
				value = static_cast<int64_t>(v.ilong);
				break;

			case sshsAttributeType::FLOAT:
				value = static_cast<float>(v.ffloat);
				break;

			case sshsAttributeType::DOUBLE:
				value = static_cast<double>(v.ddouble);
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
				cUnion.iint = static_cast<int32_t>(value);
				break;

			case sshsAttributeType::LONG:
				cUnion.ilong = static_cast<int64_t>(value);
				break;

			case sshsAttributeType::FLOAT:
				cUnion.ffloat = static_cast<float>(value);
				break;

			case sshsAttributeType::DOUBLE:
				cUnion.ddouble = static_cast<double>(value);
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
	typename sshsAttributeTypeGenerator<sshsAttributeType::STRING>::type value;

	sshsAttributeValue(typename sshsAttributeTypeGenerator<sshsAttributeType::STRING>::const_type &v) : value(v) {
	}

	sshsAttributeValue(const char *v) : value(v) {
	}

	sshsAttributeValue(const sshs_node_attr_value v) : value(v.string) {
	}

	sshs_node_attr_value getCUnion() const {
		sshs_node_attr_value cUnion;

		cUnion.string = const_cast<char *>(value.c_str());

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
				cStruct.min.iintRange = static_cast<int32_t>(min.range);
				cStruct.max.iintRange = static_cast<int32_t>(max.range);
				break;

			case sshsAttributeType::LONG:
				cStruct.min.ilongRange = static_cast<int64_t>(min.range);
				cStruct.max.ilongRange = static_cast<int64_t>(max.range);
				break;

			case sshsAttributeType::FLOAT:
				cStruct.min.ffloatRange = static_cast<float>(min.range);
				cStruct.max.ffloatRange = static_cast<float>(max.range);
				break;

			case sshsAttributeType::DOUBLE:
				cStruct.min.ddoubleRange = static_cast<double>(min.range);
				cStruct.max.ddoubleRange = static_cast<double>(max.range);
				break;

			case sshsAttributeType::STRING:
				cStruct.min.stringRange = static_cast<size_t>(min.range);
				cStruct.max.stringRange = static_cast<size_t>(max.range);
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
	void createAttribute(const std::string &key, const sshsAttributeValue<T> &defaultValue,
		const sshsAttributeRanges<T> &ranges, sshsAttributeFlags flags, const std::string &description) {
		sshsNodeCreateAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType,
			defaultValue.getCUnion(), ranges.getCStruct(), getCFlags(flags), description.c_str());
	}

	// Non-templated version for dynamic runtime types.
	void createAttribute(const std::string &key, sshsAttributeType type, const sshs_node_attr_value defaultValue,
		const sshs_node_attr_ranges &ranges, sshsAttributeFlags flags, const std::string &description) {
		sshsNodeCreateAttribute(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), defaultValue,
			ranges, getCFlags(flags), description.c_str());
	}

	template<sshsAttributeType T> void removeAttribute(const std::string &key) {
		sshsNodeRemoveAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType);
	}

	// Non-templated version for dynamic runtime types.
	void removeAttribute(const std::string &key, sshsAttributeType type) {
		sshsNodeRemoveAttribute(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type));
	}

	void removeAllAttributes() {
		sshsNodeRemoveAllAttributes(node);
	}

	template<sshsAttributeType T> bool existsAttribute(const std::string &key) {
		return (sshsNodeAttributeExists(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType));
	}

	// Non-templated version for dynamic runtime types.
	bool existsAttribute(const std::string &key, sshsAttributeType type) {
		return (sshsNodeAttributeExists(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type)));
	}

	template<sshsAttributeType T> bool putAttribute(const std::string &key, const sshsAttributeValue<T> &value) {
		return (
			sshsNodePutAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType, value.getCUnion()));
	}

	// Non-templated version for dynamic runtime types.
	bool putAttribute(const std::string &key, sshsAttributeType type, const sshs_node_attr_value value) {
		return (sshsNodePutAttribute(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), value));
	}

	template<sshsAttributeType T> sshsAttributeValue<T> getAttribute(const std::string &key) {
		return (sshsAttributeValue<T>(
			sshsNodeGetAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType)));
	}

	// Non-templated version for dynamic runtime types.
	sshs_node_attr_value getAttribute(const std::string &key, sshsAttributeType type) {
		return (sshsNodeGetAttribute(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type)));
	}

	template<sshsAttributeType T>
	bool updateReadOnlyAttribute(const std::string &key, const sshsAttributeValue<T> &value) {
		return (sshsNodeUpdateReadOnlyAttribute(
			node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType, value.getCUnion()));
	}

	// Non-templated version for dynamic runtime types.
	bool updateReadOnlyAttribute(const std::string &key, sshsAttributeType type, const sshs_node_attr_value value) {
		return (sshsNodeUpdateReadOnlyAttribute(
			node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), value));
	}

	template<sshsAttributeType T>
	void create(const std::string &key, typename sshsAttributeTypeGenerator<T>::type defaultValue,
		const sshsAttributeRanges<T> &ranges, sshsAttributeFlags flags, const std::string &description) {
		const sshsAttributeValue<T> defVal(defaultValue);
		sshsNodeCreateAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType, defVal.getCUnion(),
			ranges.getCStruct(), getCFlags(flags), description.c_str());
	}

	template<sshsAttributeType T> bool put(const std::string &key, typename sshsAttributeTypeGenerator<T>::type value) {
		const sshsAttributeValue<T> val(value);
		return (
			sshsNodePutAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType, val.getCUnion()));
	}

	template<sshsAttributeType T> typename sshsAttributeTypeGenerator<T>::type get(const std::string &key) {
		return (sshsAttributeValue<T>(
			sshsNodeGetAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType))
					.value);
	}

	bool exportNodeToXML(int fd) {
		return (sshsNodeExportNodeToXML(node, fd));
	}

	bool exportSubTreeToXML(int fd) {
		return (sshsNodeExportSubTreeToXML(node, fd));
	}

	bool importNodeFromXML(int fd, bool strict = true) {
		return (sshsNodeImportNodeFromXML(node, fd, strict));
	}

	bool importSubTreeFromXML(int fd, bool strict = true) {
		return (sshsNodeImportSubTreeFromXML(node, fd, strict));
	}

	bool stringToAttributeConverter(const std::string &key, const std::string &type, const std::string &value) {
		return (sshsNodeStringToAttributeConverter(node, key.c_str(), type.c_str(), value.c_str()));
	}

	std::vector<std::string> getChildNames() {
		std::vector<std::string> childNames;

		size_t numChildNames         = 0;
		const char **childNamesArray = sshsNodeGetChildNames(node, &numChildNames);

		if (numChildNames > 0) {
			childNames.reserve(numChildNames);

			for (size_t i = 0; i < numChildNames; i++) {
				childNames.emplace_back(childNamesArray[i]);
			}

			free(childNamesArray);
		}

		return (childNames);
	}

	std::vector<std::string> getAttributeKeys() {
		std::vector<std::string> attributeKeys;

		size_t numAttributeKeys         = 0;
		const char **attributeKeysArray = sshsNodeGetAttributeKeys(node, &numAttributeKeys);

		if (numAttributeKeys > 0) {
			attributeKeys.reserve(numAttributeKeys);

			for (size_t i = 0; i < numAttributeKeys; i++) {
				attributeKeys.emplace_back(attributeKeysArray[i]);
			}

			free(attributeKeysArray);
		}

		return (attributeKeys);
	}

	sshsAttributeType getAttributeType(const std::string &key) {
		return (static_cast<sshsAttributeType>(sshsNodeGetAttributeType(node, key.c_str())));
	}

	// sshsAttributeRanges getAttributeRanges(const std::string &key) {
	//}

	int getAttributeFlags(const std::string &key) {
	}

	template<sshsAttributeType T> std::string getAttributeDescription(const std::string &key) {
		return (sshsNodeGetAttributeDescription(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType));
	}

	// Non-templated version for dynamic runtime types.
	std::string getAttributeDescription(const std::string &key, sshsAttributeType type) {
		return (sshsNodeGetAttributeDescription(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type)));
	}

	void createAttributeListOptions(
		const std::string &key, const std::string &listOptions, bool allowMultipleSelections) {
		sshsNodeCreateAttributeListOptions(node, key.c_str(), listOptions.c_str(), allowMultipleSelections);
	}

	void createAttributeFileChooser(const std::string &key, const std::string &allowedExtensions) {
		sshsNodeCreateAttributeFileChooser(node, key.c_str(), allowedExtensions.c_str());
	}
};

class sshsc {
	sshsc() {
		sshsn(nullptr).createAttribute<sshsAttributeType::STRING>(
			"baa", "default", {10, 20}, sshsAttributeFlags::NORMAL | sshsAttributeFlags::NO_EXPORT, "raa");
		sshsn(nullptr).createAttribute<sshsAttributeType::FLOAT>(
			"baa", 12.10f, {10, 20}, sshsAttributeFlags::NORMAL | sshsAttributeFlags::NO_EXPORT, "raa");
		sshsn(nullptr).createAttribute<sshsAttributeType::BOOL>(
			"baa", true, {}, sshsAttributeFlags::NORMAL | sshsAttributeFlags::NO_EXPORT, "raa");

		auto aa = sshsn(nullptr).getAttribute<sshsAttributeType::STRING>("baa");
		auto bb = sshsn(nullptr).getAttribute<sshsAttributeType::FLOAT>("baa");
		auto cc = sshsn(nullptr).getAttribute<sshsAttributeType::BOOL>("baa");

		sshsn(nullptr).putAttribute<sshsAttributeType::STRING>("baa", "default");
		sshsn(nullptr).putAttribute<sshsAttributeType::FLOAT>("baa", 12.10f);
		sshsn(nullptr).putAttribute<sshsAttributeType::BOOL>("baa", true);

		sshsn(nullptr).create<sshsAttributeType::STRING>(
			"baa", "default", {10, 20}, sshsAttributeFlags::NORMAL | sshsAttributeFlags::NO_EXPORT, "raa");
		sshsn(nullptr).create<sshsAttributeType::FLOAT>(
			"baa", 12.10f, {10, 20}, sshsAttributeFlags::NORMAL | sshsAttributeFlags::NO_EXPORT, "raa");
		sshsn(nullptr).create<sshsAttributeType::BOOL>(
			"baa", true, {}, sshsAttributeFlags::NORMAL | sshsAttributeFlags::NO_EXPORT, "raa");

		std::string aaa = sshsn(nullptr).get<sshsAttributeType::STRING>("baa");
		float bbb       = sshsn(nullptr).get<sshsAttributeType::FLOAT>("baa");
		bool ccc        = sshsn(nullptr).get<sshsAttributeType::BOOL>("baa");

		sshsn(nullptr).put<sshsAttributeType::STRING>("baa", "default");
		sshsn(nullptr).put<sshsAttributeType::FLOAT>("baa", 12.10f);
		sshsn(nullptr).put<sshsAttributeType::BOOL>("baa", true);
	}

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
