#ifndef SSHS_HPP_
#define SSHS_HPP_

#include "sshs.h"

#include <string>
#include <vector>

namespace dv {
namespace Config {

enum class sshsAttributeTypeS {
	UNKNOWN = SSHS_UNKNOWN,
	BOOL    = SSHS_BOOL,
	INT     = SSHS_INT,
	LONG    = SSHS_LONG,
	FLOAT   = SSHS_FLOAT,
	DOUBLE  = SSHS_DOUBLE,
	STRING  = SSHS_STRING,
};

template<sshsAttributeTypeS T> struct sshsAttributeTypeGenerator {
	static_assert(true, "Only BOOL, INT, LONG, FLOAT, DOUBLE and STRING are supported.");
};

template<> struct sshsAttributeTypeGenerator<sshsAttributeTypeS::BOOL> {
	using type                                                 = bool;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_BOOL;
};

template<> struct sshsAttributeTypeGenerator<sshsAttributeTypeS::INT> {
	using type                                                 = int32_t;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_INT;
};

template<> struct sshsAttributeTypeGenerator<sshsAttributeTypeS::LONG> {
	using type                                                 = int64_t;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_LONG;
};

template<> struct sshsAttributeTypeGenerator<sshsAttributeTypeS::FLOAT> {
	using type                                                 = float;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_FLOAT;
};

template<> struct sshsAttributeTypeGenerator<sshsAttributeTypeS::DOUBLE> {
	using type                                                 = double;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_DOUBLE;
};

template<> struct sshsAttributeTypeGenerator<sshsAttributeTypeS::STRING> {
	using type                                                 = std::string;
	using const_type                                           = const std::string;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_STRING;
};

template<sshsAttributeTypeS T> struct sshsAttributeRangeGenerator {
	using rangeType = typename sshsAttributeTypeGenerator<T>::type;
};

template<> struct sshsAttributeRangeGenerator<sshsAttributeTypeS::BOOL> { using rangeType = size_t; };

template<> struct sshsAttributeRangeGenerator<sshsAttributeTypeS::STRING> { using rangeType = size_t; };

template<sshsAttributeTypeS T> struct sshsAttributeValueT {
public:
	typename sshsAttributeTypeGenerator<T>::type value;

	sshsAttributeValueT(typename sshsAttributeTypeGenerator<T>::type v) : value(v) {
	}

	sshsAttributeValueT(const sshs_node_attr_value v) {
		switch (T) {
			case sshsAttributeTypeS::BOOL:
				value = v.boolean;
				break;

			case sshsAttributeTypeS::INT:
				value = static_cast<int32_t>(v.iint);
				break;

			case sshsAttributeTypeS::LONG:
				value = static_cast<int64_t>(v.ilong);
				break;

			case sshsAttributeTypeS::FLOAT:
				value = static_cast<float>(v.ffloat);
				break;

			case sshsAttributeTypeS::DOUBLE:
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
			case sshsAttributeTypeS::BOOL:
				cUnion.boolean = value;
				break;

			case sshsAttributeTypeS::INT:
				cUnion.iint = static_cast<int32_t>(value);
				break;

			case sshsAttributeTypeS::LONG:
				cUnion.ilong = static_cast<int64_t>(value);
				break;

			case sshsAttributeTypeS::FLOAT:
				cUnion.ffloat = static_cast<float>(value);
				break;

			case sshsAttributeTypeS::DOUBLE:
				cUnion.ddouble = static_cast<double>(value);
				break;

			default:
				static_assert(true, "sshsAttributeValue<STRING> is specialized.");
				break;
		}

		return (cUnion);
	}
};

template<> struct sshsAttributeValueT<sshsAttributeTypeS::STRING> {
public:
	typename sshsAttributeTypeGenerator<sshsAttributeTypeS::STRING>::type value;

	sshsAttributeValueT(typename sshsAttributeTypeGenerator<sshsAttributeTypeS::STRING>::const_type &v) : value(v) {
	}

	sshsAttributeValueT(const char *v) : value(v) {
	}

	sshsAttributeValueT(const sshs_node_attr_value v) : value(v.string) {
	}

	sshs_node_attr_value getCUnion() const {
		sshs_node_attr_value cUnion;

		cUnion.string = const_cast<char *>(value.c_str());

		return (cUnion);
	}
};

template<sshsAttributeTypeS T> struct sshsAttributeRangeT {
public:
	typename sshsAttributeRangeGenerator<T>::rangeType range;

	sshsAttributeRangeT(typename sshsAttributeRangeGenerator<T>::rangeType r) : range(r) {
	}
};

template<sshsAttributeTypeS T> struct sshsAttributeRangesT {
public:
	sshsAttributeRangeT<T> min;
	sshsAttributeRangeT<T> max;

	sshsAttributeRangesT(typename sshsAttributeRangeGenerator<T>::rangeType minVal,
		typename sshsAttributeRangeGenerator<T>::rangeType maxVal) :
		min(minVal),
		max(maxVal) {
	}

	sshsAttributeRangesT(const sshs_node_attr_ranges ranges) {
		switch (T) {
			case sshsAttributeTypeS::INT:
				min.range = static_cast<int32_t>(ranges.min.iintRange);
				max.range = static_cast<int32_t>(ranges.max.iintRange);
				break;

			case sshsAttributeTypeS::LONG:
				min.range = static_cast<int64_t>(ranges.min.ilongRange);
				max.range = static_cast<int64_t>(ranges.max.ilongRange);
				break;

			case sshsAttributeTypeS::FLOAT:
				min.range = static_cast<float>(ranges.min.ffloatRange);
				max.range = static_cast<float>(ranges.max.ffloatRange);
				break;

			case sshsAttributeTypeS::DOUBLE:
				min.range = static_cast<double>(ranges.min.ddoubleRange);
				max.range = static_cast<double>(ranges.max.ddoubleRange);
				break;

			case sshsAttributeTypeS::STRING:
				min.range = static_cast<size_t>(ranges.min.stringRange);
				max.range = static_cast<size_t>(ranges.max.stringRange);
				break;

			default:
				static_assert(true, "sshsAttributeRanges<BOOL> is specialized.");
				break;
		}
	}

	sshs_node_attr_ranges getCStruct() const {
		sshs_node_attr_ranges cStruct;

		switch (T) {
			case sshsAttributeTypeS::INT:
				cStruct.min.iintRange = static_cast<int32_t>(min.range);
				cStruct.max.iintRange = static_cast<int32_t>(max.range);
				break;

			case sshsAttributeTypeS::LONG:
				cStruct.min.ilongRange = static_cast<int64_t>(min.range);
				cStruct.max.ilongRange = static_cast<int64_t>(max.range);
				break;

			case sshsAttributeTypeS::FLOAT:
				cStruct.min.ffloatRange = static_cast<float>(min.range);
				cStruct.max.ffloatRange = static_cast<float>(max.range);
				break;

			case sshsAttributeTypeS::DOUBLE:
				cStruct.min.ddoubleRange = static_cast<double>(min.range);
				cStruct.max.ddoubleRange = static_cast<double>(max.range);
				break;

			case sshsAttributeTypeS::STRING:
				cStruct.min.stringRange = static_cast<size_t>(min.range);
				cStruct.max.stringRange = static_cast<size_t>(max.range);
				break;

			default:
				static_assert(true, "sshsAttributeRanges<BOOL> is specialized.");
				break;
		}

		return (cStruct);
	}
};

template<> struct sshsAttributeRangesT<sshsAttributeTypeS::BOOL> {
public:
	sshsAttributeRangeT<sshsAttributeTypeS::BOOL> min;
	sshsAttributeRangeT<sshsAttributeTypeS::BOOL> max;

	sshsAttributeRangesT() : min(0), max(0) {
	}

	sshsAttributeRangesT(const sshs_node_attr_ranges ranges) : min(0), max(0) {
		UNUSED_ARGUMENT(ranges); // Ignore ranges for bool, always zero.
	}

	sshs_node_attr_ranges getCStruct() const {
		sshs_node_attr_ranges cStruct;

		cStruct.min.stringRange = 0;
		cStruct.max.stringRange = 0;

		return (cStruct);
	}
};

enum class sshsAttributeFlagsS {
	NORMAL      = SSHS_FLAGS_NORMAL,
	READ_ONLY   = SSHS_FLAGS_READ_ONLY,
	NOTIFY_ONLY = SSHS_FLAGS_NOTIFY_ONLY,
	NO_EXPORT   = SSHS_FLAGS_NO_EXPORT,
};

inline sshsAttributeFlagsS operator|(sshsAttributeFlagsS lhs, sshsAttributeFlagsS rhs) {
	using T = std::underlying_type_t<sshsAttributeFlagsS>;
	return (static_cast<sshsAttributeFlagsS>(static_cast<T>(lhs) | static_cast<T>(rhs)));
}

inline sshsAttributeFlagsS &operator|=(sshsAttributeFlagsS &lhs, sshsAttributeFlagsS rhs) {
	lhs = lhs | rhs; // Call | overload above.
	return (lhs);
}

inline int getCFlags(sshsAttributeFlagsS f) {
	return (static_cast<std::underlying_type_t<sshsAttributeFlagsS>>(f));
}

class Node {
private:
	sshsNode node;

public:
	Node(sshsNode n) : node(n) {
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
	Node getParent() {
		return (sshsNodeGetParent(node));
	}

	/**
	 * This returns a reference to a node, and as such must be carefully mediated with
	 * any sshsNodeRemoveNode() calls.
	 */
	std::vector<Node> getChildren() {
		std::vector<Node> children;

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

	template<sshsAttributeTypeS T>
	void createAttribute(const std::string &key, const sshsAttributeValueT<T> &defaultValue,
		const sshsAttributeRangesT<T> &ranges, sshsAttributeFlagsS flags, const std::string &description) {
		sshsNodeCreateAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType,
			defaultValue.getCUnion(), ranges.getCStruct(), getCFlags(flags), description.c_str());
	}

	// Non-templated version for dynamic runtime types.
	void createAttribute(const std::string &key, sshsAttributeTypeS type, const sshs_node_attr_value defaultValue,
		const sshs_node_attr_ranges &ranges, sshsAttributeFlagsS flags, const std::string &description) {
		sshsNodeCreateAttribute(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), defaultValue,
			ranges, getCFlags(flags), description.c_str());
	}

	template<sshsAttributeTypeS T> void removeAttribute(const std::string &key) {
		sshsNodeRemoveAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType);
	}

	// Non-templated version for dynamic runtime types.
	void removeAttribute(const std::string &key, sshsAttributeTypeS type) {
		sshsNodeRemoveAttribute(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type));
	}

	void removeAllAttributes() {
		sshsNodeRemoveAllAttributes(node);
	}

	template<sshsAttributeTypeS T> bool existsAttribute(const std::string &key) {
		return (sshsNodeAttributeExists(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType));
	}

	// Non-templated version for dynamic runtime types.
	bool existsAttribute(const std::string &key, sshsAttributeTypeS type) {
		return (sshsNodeAttributeExists(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type)));
	}

	template<sshsAttributeTypeS T> bool putAttribute(const std::string &key, const sshsAttributeValueT<T> &value) {
		return (
			sshsNodePutAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType, value.getCUnion()));
	}

	// Non-templated version for dynamic runtime types.
	bool putAttribute(const std::string &key, sshsAttributeTypeS type, const sshs_node_attr_value value) {
		return (sshsNodePutAttribute(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), value));
	}

	template<sshsAttributeTypeS T> sshsAttributeValueT<T> getAttribute(const std::string &key) {
		sshs_node_attr_value cVal
			= sshsNodeGetAttribute(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType);

		sshsAttributeValueT<T> retVal(cVal);

		if (T == sshsAttributeTypeS::STRING) {
			free(cVal.string);
		}

		return (retVal);
	}

	// Non-templated version for dynamic runtime types. Remember to free retVal.string if type == STRING.
	sshs_node_attr_value getAttribute(const std::string &key, sshsAttributeTypeS type) {
		return (sshsNodeGetAttribute(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type)));
	}

	template<sshsAttributeTypeS T>
	bool updateReadOnlyAttribute(const std::string &key, const sshsAttributeValueT<T> &value) {
		return (sshsNodeUpdateReadOnlyAttribute(
			node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType, value.getCUnion()));
	}

	// Non-templated version for dynamic runtime types.
	bool updateReadOnlyAttribute(const std::string &key, sshsAttributeTypeS type, const sshs_node_attr_value value) {
		return (sshsNodeUpdateReadOnlyAttribute(
			node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), value));
	}

	template<sshsAttributeTypeS T>
	void create(const std::string &key, typename sshsAttributeTypeGenerator<T>::type defaultValue,
		const sshsAttributeRangesT<T> &ranges, sshsAttributeFlagsS flags, const std::string &description) {
		const sshsAttributeValueT<T> defVal(defaultValue);
		createAttribute(key, defVal, ranges, flags, description);
	}

	template<sshsAttributeTypeS T>
	bool put(const std::string &key, typename sshsAttributeTypeGenerator<T>::type value) {
		const sshsAttributeValueT<T> val(value);
		return (putAttribute<T>(key, val));
	}

	template<sshsAttributeTypeS T> typename sshsAttributeTypeGenerator<T>::type get(const std::string &key) {
		return (getAttribute<T>(key).value);
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

	sshsAttributeTypeS getAttributeType(const std::string &key) {
		return (static_cast<sshsAttributeTypeS>(sshsNodeGetAttributeType(node, key.c_str())));
	}

	template<sshsAttributeTypeS T> sshsAttributeRangesT<T> getAttributeRanges(const std::string &key) {
		return (sshsNodeGetAttributeRanges(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType));
	}

	// Non-templated version for dynamic runtime types.
	struct sshs_node_attr_ranges getAttributeRanges(const std::string &key, sshsAttributeTypeS type) {
		return (sshsNodeGetAttributeRanges(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type)));
	}

	template<sshsAttributeTypeS T> int getAttributeFlags(const std::string &key) {
		return (sshsNodeGetAttributeFlags(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType));
	}

	// Non-templated version for dynamic runtime types.
	int getAttributeFlags(const std::string &key, sshsAttributeTypeS type) {
		return (sshsNodeGetAttributeFlags(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type)));
	}

	template<sshsAttributeTypeS T> std::string getAttributeDescription(const std::string &key) {
		char *desc = sshsNodeGetAttributeDescription(node, key.c_str(), sshsAttributeTypeGenerator<T>::underlyingType);

		std::string retVal(desc);

		free(desc);

		return (retVal);
	}

	// Non-templated version for dynamic runtime types.
	std::string getAttributeDescription(const std::string &key, sshsAttributeTypeS type) {
		char *desc
			= sshsNodeGetAttributeDescription(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type));

		std::string retVal(desc);

		free(desc);

		return (retVal);
	}

	void createAttributeListOptions(
		const std::string &key, const std::string &listOptions, bool allowMultipleSelections) {
		sshsNodeCreateAttributeListOptions(node, key.c_str(), listOptions.c_str(), allowMultipleSelections);
	}

	void createAttributeFileChooser(const std::string &key, const std::string &allowedExtensions) {
		sshsNodeCreateAttributeFileChooser(node, key.c_str(), allowedExtensions.c_str());
	}

	bool existsRelativeNode(const std::string &nodePath) {
		return (sshsExistsRelativeNode(node, nodePath.c_str()));
	}

	/**
	 * This returns a reference to a node, and as such must be carefully mediated with
	 * any sshsNodeRemoveNode() calls.
	 */
	Node getRelativeNode(const std::string &nodePath) {
		return (sshsGetRelativeNode(node, nodePath.c_str()));
	}

	void attributeUpdaterAdd(
		const std::string &key, sshsAttributeTypeS type, sshsAttributeUpdater updater, void *updaterUserData) {
		sshsAttributeUpdaterAdd(
			node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), updater, updaterUserData);
	}

	void attributeUpdaterRemove(
		const std::string &key, sshsAttributeTypeS type, sshsAttributeUpdater updater, void *updaterUserData) {
		sshsAttributeUpdaterRemove(
			node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), updater, updaterUserData);
	}

	void attributeUpdaterRemoveAllForNode() {
		sshsAttributeUpdaterRemoveAllForNode(node);
	}
};

class Helper {
public:
	static std::string typeToStringConverter(sshsAttributeTypeS type) {
		return (sshsHelperTypeToStringConverter(static_cast<enum sshs_node_attr_value_type>(type)));
	}

	static sshsAttributeTypeS stringToTypeConverter(const std::string &typeString) {
		return (static_cast<sshsAttributeTypeS>(sshsHelperStringToTypeConverter(typeString.c_str())));
	}

	static std::string valueToStringConverter(sshsAttributeTypeS type, union sshs_node_attr_value value) {
		char *val = sshsHelperValueToStringConverter(static_cast<enum sshs_node_attr_value_type>(type), value);

		std::string retVal(val);

		free(val);

		return (retVal);
	}

	static union sshs_node_attr_value stringToValueConverter(sshsAttributeTypeS type, const std::string &valueString) {
		return (
			sshsHelperStringToValueConverter(static_cast<enum sshs_node_attr_value_type>(type), valueString.c_str()));
	}

	static std::string flagsToStringConverter(int flags) {
		char *val = sshsHelperFlagsToStringConverter(flags);

		std::string retVal(val);

		free(val);

		return (retVal);
	}

	static int stringToFlagsConverter(const std::string &flagsString) {
		return (sshsHelperStringToFlagsConverter(flagsString.c_str()));
	}

	static std::string rangesToStringConverter(sshsAttributeTypeS type, struct sshs_node_attr_ranges ranges) {
		char *val = sshsHelperRangesToStringConverter(static_cast<enum sshs_node_attr_value_type>(type), ranges);

		std::string retVal(val);

		free(val);

		return (retVal);
	}

	static struct sshs_node_attr_ranges stringToRangesConverter(
		sshsAttributeTypeS type, const std::string &rangesString) {
		return (
			sshsHelperStringToRangesConverter(static_cast<enum sshs_node_attr_value_type>(type), rangesString.c_str()));
	}
};

class Tree {
private:
	sshs tree;

public:
	Tree(sshs t) : tree(t) {
	}

	static Tree getGlobal() {
		return (sshsGetGlobal());
	}

	static void setGlobalErrorLogCallback(sshsErrorLogCallback error_log_cb) {
		sshsSetGlobalErrorLogCallback(error_log_cb);
	}

	static sshsErrorLogCallback getGlobalErrorLogCallback() {
		return (sshsGetGlobalErrorLogCallback());
	}

	bool existsNode(const std::string &nodePath) {
		return (sshsExistsNode(tree, nodePath.c_str()));
	}

	/**
	 * This returns a reference to a node, and as such must be carefully mediated with
	 * any sshsNodeRemoveNode() calls.
	 */
	Node getNode(const std::string &nodePath) {
		return (sshsGetNode(tree, nodePath.c_str()));
	}

	void attributeUpdaterRemoveAll() {
		sshsAttributeUpdaterRemoveAll(tree);
	}

	bool attributeUpdaterRun() {
		return (sshsAttributeUpdaterRun(tree));
	}

	/**
	 * Listener must be able to deal with userData being NULL at any moment.
	 * This can happen due to concurrent changes from this setter.
	 */
	void globalNodeListenerSet(sshsNodeChangeListener node_changed, void *userData) {
		sshsGlobalNodeListenerSet(tree, node_changed, userData);
	}

	/**
	 * Listener must be able to deal with userData being NULL at any moment.
	 * This can happen due to concurrent changes from this setter.
	 */
	void globalAttributeListenerSet(sshsAttributeChangeListener attribute_changed, void *userData) {
		sshsGlobalAttributeListenerSet(tree, attribute_changed, userData);
	}
};

} // namespace Config
} // namespace dv

#endif /* SSHS_HPP_ */
