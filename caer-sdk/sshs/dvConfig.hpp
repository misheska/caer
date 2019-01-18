#ifndef DVCONFIG_HPP_
#define DVCONFIG_HPP_

#include "sshs.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace dv {
namespace Config {

enum class AttributeType {
	UNKNOWN = SSHS_UNKNOWN,
	BOOL    = SSHS_BOOL,
	INT     = SSHS_INT,
	LONG    = SSHS_LONG,
	FLOAT   = SSHS_FLOAT,
	DOUBLE  = SSHS_DOUBLE,
	STRING  = SSHS_STRING,
};

template<AttributeType T> struct AttributeTypeGenerator {
	static_assert(true, "Only BOOL, INT, LONG, FLOAT, DOUBLE and STRING are supported.");
};

template<> struct AttributeTypeGenerator<AttributeType::BOOL> {
	using type                                                 = bool;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_BOOL;
};

template<> struct AttributeTypeGenerator<AttributeType::INT> {
	using type                                                 = int32_t;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_INT;
};

template<> struct AttributeTypeGenerator<AttributeType::LONG> {
	using type                                                 = int64_t;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_LONG;
};

template<> struct AttributeTypeGenerator<AttributeType::FLOAT> {
	using type                                                 = float;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_FLOAT;
};

template<> struct AttributeTypeGenerator<AttributeType::DOUBLE> {
	using type                                                 = double;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_DOUBLE;
};

template<> struct AttributeTypeGenerator<AttributeType::STRING> {
	using type                                                 = std::string;
	using const_type                                           = const std::string;
	static const enum sshs_node_attr_value_type underlyingType = SSHS_STRING;
};

template<AttributeType T> struct AttributeRangeGenerator {
	using rangeType = typename AttributeTypeGenerator<T>::type;
};

template<> struct AttributeRangeGenerator<AttributeType::BOOL> { using rangeType = size_t; };

template<> struct AttributeRangeGenerator<AttributeType::STRING> { using rangeType = size_t; };

template<AttributeType T> struct AttributeValue {
public:
	typename AttributeTypeGenerator<T>::type value;

	AttributeValue(typename AttributeTypeGenerator<T>::type v) : value(v) {
	}

	AttributeValue(const sshs_node_attr_value v) {
		switch (T) {
			case AttributeType::BOOL:
				value = v.boolean;
				break;

			case AttributeType::INT:
				value = static_cast<int32_t>(v.iint);
				break;

			case AttributeType::LONG:
				value = static_cast<int64_t>(v.ilong);
				break;

			case AttributeType::FLOAT:
				value = static_cast<float>(v.ffloat);
				break;

			case AttributeType::DOUBLE:
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
			case AttributeType::BOOL:
				cUnion.boolean = value;
				break;

			case AttributeType::INT:
				cUnion.iint = static_cast<int32_t>(value);
				break;

			case AttributeType::LONG:
				cUnion.ilong = static_cast<int64_t>(value);
				break;

			case AttributeType::FLOAT:
				cUnion.ffloat = static_cast<float>(value);
				break;

			case AttributeType::DOUBLE:
				cUnion.ddouble = static_cast<double>(value);
				break;

			default:
				static_assert(true, "sshsAttributeValue<STRING> is specialized.");
				break;
		}

		return (cUnion);
	}
};

template<> struct AttributeValue<AttributeType::STRING> {
public:
	typename AttributeTypeGenerator<AttributeType::STRING>::type value;

	AttributeValue(typename AttributeTypeGenerator<AttributeType::STRING>::const_type &v) : value(v) {
	}

	AttributeValue(const char *v) : value(v) {
	}

	AttributeValue(const sshs_node_attr_value v) : value(v.string) {
	}

	sshs_node_attr_value getCUnion() const {
		sshs_node_attr_value cUnion;

		cUnion.string = const_cast<char *>(value.c_str());

		return (cUnion);
	}
};

template<AttributeType T> struct AttributeRange {
public:
	typename AttributeRangeGenerator<T>::rangeType range;

	AttributeRange(typename AttributeRangeGenerator<T>::rangeType r) : range(r) {
	}
};

template<AttributeType T> struct AttributeRanges {
public:
	AttributeRange<T> min;
	AttributeRange<T> max;

	AttributeRanges(
		typename AttributeRangeGenerator<T>::rangeType minVal, typename AttributeRangeGenerator<T>::rangeType maxVal) :
		min(minVal),
		max(maxVal) {
	}

	AttributeRanges(const sshs_node_attr_ranges ranges) {
		switch (T) {
			case AttributeType::INT:
				min.range = static_cast<int32_t>(ranges.min.iintRange);
				max.range = static_cast<int32_t>(ranges.max.iintRange);
				break;

			case AttributeType::LONG:
				min.range = static_cast<int64_t>(ranges.min.ilongRange);
				max.range = static_cast<int64_t>(ranges.max.ilongRange);
				break;

			case AttributeType::FLOAT:
				min.range = static_cast<float>(ranges.min.ffloatRange);
				max.range = static_cast<float>(ranges.max.ffloatRange);
				break;

			case AttributeType::DOUBLE:
				min.range = static_cast<double>(ranges.min.ddoubleRange);
				max.range = static_cast<double>(ranges.max.ddoubleRange);
				break;

			case AttributeType::STRING:
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
			case AttributeType::INT:
				cStruct.min.iintRange = static_cast<int32_t>(min.range);
				cStruct.max.iintRange = static_cast<int32_t>(max.range);
				break;

			case AttributeType::LONG:
				cStruct.min.ilongRange = static_cast<int64_t>(min.range);
				cStruct.max.ilongRange = static_cast<int64_t>(max.range);
				break;

			case AttributeType::FLOAT:
				cStruct.min.ffloatRange = static_cast<float>(min.range);
				cStruct.max.ffloatRange = static_cast<float>(max.range);
				break;

			case AttributeType::DOUBLE:
				cStruct.min.ddoubleRange = static_cast<double>(min.range);
				cStruct.max.ddoubleRange = static_cast<double>(max.range);
				break;

			case AttributeType::STRING:
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

template<> struct AttributeRanges<AttributeType::BOOL> {
public:
	AttributeRange<AttributeType::BOOL> min;
	AttributeRange<AttributeType::BOOL> max;

	AttributeRanges() : min(0), max(0) {
	}

	AttributeRanges(const sshs_node_attr_ranges ranges) : min(0), max(0) {
		(void) (ranges); // Ignore ranges for bool, always zero.
	}

	sshs_node_attr_ranges getCStruct() const {
		sshs_node_attr_ranges cStruct;

		cStruct.min.stringRange = 0;
		cStruct.max.stringRange = 0;

		return (cStruct);
	}
};

enum class AttributeFlags {
	NORMAL      = SSHS_FLAGS_NORMAL,
	READ_ONLY   = SSHS_FLAGS_READ_ONLY,
	NOTIFY_ONLY = SSHS_FLAGS_NOTIFY_ONLY,
	NO_EXPORT   = SSHS_FLAGS_NO_EXPORT,
};

inline AttributeFlags operator|(AttributeFlags lhs, AttributeFlags rhs) {
	using T = std::underlying_type_t<AttributeFlags>;
	return (static_cast<AttributeFlags>(static_cast<T>(lhs) | static_cast<T>(rhs)));
}

inline AttributeFlags &operator|=(AttributeFlags &lhs, AttributeFlags rhs) {
	lhs = lhs | rhs; // Call | overload above.
	return (lhs);
}

inline int getCFlags(AttributeFlags f) {
	return (static_cast<std::underlying_type_t<AttributeFlags>>(f));
}

class Node {
private:
	sshsNode node;

public:
	Node(sshsNode n) : node(n) {
	}

	explicit operator sshsNode() const {
		return (node);
	}

	std::string getName() const {
		return (sshsNodeGetName(node));
	}

	std::string getPath() const {
		return (sshsNodeGetPath(node));
	}

	/**
	 * This returns a reference to a node, and as such must be carefully mediated with
	 * any sshsNodeRemoveNode() calls.
	 *
	 * @throws std::out_of_range Node is root and has no parent.
	 */
	Node getParent() const {
		sshsNode parent = sshsNodeGetParent(node);

		if (parent == nullptr) {
			throw std::out_of_range("Node is root and has no parent.");
		}

		return (parent);
	}

	/**
	 * This returns a reference to a node, and as such must be carefully mediated with
	 * any sshsNodeRemoveNode() calls.
	 */
	std::vector<Node> getChildren() const {
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

	template<AttributeType T>
	void createAttribute(const std::string &key, const AttributeValue<T> &defaultValue,
		const AttributeRanges<T> &ranges, AttributeFlags flags, const std::string &description) {
		sshsNodeCreateAttribute(node, key.c_str(), AttributeTypeGenerator<T>::underlyingType, defaultValue.getCUnion(),
			ranges.getCStruct(), getCFlags(flags), description.c_str());
	}

	// Non-templated version for dynamic runtime types.
	void createAttribute(const std::string &key, AttributeType type, const sshs_node_attr_value defaultValue,
		const sshs_node_attr_ranges &ranges, AttributeFlags flags, const std::string &description) {
		sshsNodeCreateAttribute(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), defaultValue,
			ranges, getCFlags(flags), description.c_str());
	}

	template<AttributeType T> void removeAttribute(const std::string &key) {
		sshsNodeRemoveAttribute(node, key.c_str(), AttributeTypeGenerator<T>::underlyingType);
	}

	// Non-templated version for dynamic runtime types.
	void removeAttribute(const std::string &key, AttributeType type) {
		sshsNodeRemoveAttribute(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type));
	}

	void removeAllAttributes() {
		sshsNodeRemoveAllAttributes(node);
	}

	template<AttributeType T> bool existsAttribute(const std::string &key) const {
		return (sshsNodeAttributeExists(node, key.c_str(), AttributeTypeGenerator<T>::underlyingType));
	}

	// Non-templated version for dynamic runtime types.
	bool existsAttribute(const std::string &key, AttributeType type) const {
		return (sshsNodeAttributeExists(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type)));
	}

	template<AttributeType T> bool putAttribute(const std::string &key, const AttributeValue<T> &value) {
		return (sshsNodePutAttribute(node, key.c_str(), AttributeTypeGenerator<T>::underlyingType, value.getCUnion()));
	}

	// Non-templated version for dynamic runtime types.
	bool putAttribute(const std::string &key, AttributeType type, const sshs_node_attr_value value) {
		return (sshsNodePutAttribute(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), value));
	}

	template<AttributeType T> AttributeValue<T> getAttribute(const std::string &key) const {
		sshs_node_attr_value cVal = sshsNodeGetAttribute(node, key.c_str(), AttributeTypeGenerator<T>::underlyingType);

		AttributeValue<T> retVal(cVal);

		if (T == AttributeType::STRING) {
			free(cVal.string);
		}

		return (retVal);
	}

	// Non-templated version for dynamic runtime types. Remember to free retVal.string if type == STRING.
	sshs_node_attr_value getAttribute(const std::string &key, AttributeType type) const {
		return (sshsNodeGetAttribute(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type)));
	}

	template<AttributeType T> bool updateReadOnlyAttribute(const std::string &key, const AttributeValue<T> &value) {
		return (sshsNodeUpdateReadOnlyAttribute(
			node, key.c_str(), AttributeTypeGenerator<T>::underlyingType, value.getCUnion()));
	}

	// Non-templated version for dynamic runtime types.
	bool updateReadOnlyAttribute(const std::string &key, AttributeType type, const sshs_node_attr_value value) {
		return (sshsNodeUpdateReadOnlyAttribute(
			node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), value));
	}

	template<AttributeType T>
	void create(const std::string &key, typename AttributeTypeGenerator<T>::type defaultValue,
		const AttributeRanges<T> &ranges, AttributeFlags flags, const std::string &description) {
		const AttributeValue<T> defVal(defaultValue);
		createAttribute(key, defVal, ranges, flags, description);
	}

	template<AttributeType T> void remove(const std::string &key) {
		removeAttribute<T>(key);
	}

	template<AttributeType T> bool exists(const std::string &key) const {
		return (existsAttribute<T>(key));
	}

	template<AttributeType T> bool put(const std::string &key, typename AttributeTypeGenerator<T>::type value) {
		const AttributeValue<T> val(value);
		return (putAttribute<T>(key, val));
	}

	template<AttributeType T>
	bool updateReadOnly(const std::string &key, typename AttributeTypeGenerator<T>::type value) {
		const AttributeValue<T> val(value);
		return (updateReadOnlyAttribute<T>(key, val));
	}

	template<AttributeType T> typename AttributeTypeGenerator<T>::type get(const std::string &key) const {
		return (getAttribute<T>(key).value);
	}

	bool exportNodeToXML(int fd) const {
		return (sshsNodeExportNodeToXML(node, fd));
	}

	bool exportSubTreeToXML(int fd) const {
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

	std::vector<std::string> getChildNames() const {
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

	std::vector<std::string> getAttributeKeys() const {
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

	AttributeType getAttributeType(const std::string &key) const {
		return (static_cast<AttributeType>(sshsNodeGetAttributeType(node, key.c_str())));
	}

	template<AttributeType T> AttributeRanges<T> getAttributeRanges(const std::string &key) const {
		return (sshsNodeGetAttributeRanges(node, key.c_str(), AttributeTypeGenerator<T>::underlyingType));
	}

	// Non-templated version for dynamic runtime types.
	struct sshs_node_attr_ranges getAttributeRanges(const std::string &key, AttributeType type) const {
		return (sshsNodeGetAttributeRanges(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type)));
	}

	template<AttributeType T> int getAttributeFlags(const std::string &key) const {
		return (sshsNodeGetAttributeFlags(node, key.c_str(), AttributeTypeGenerator<T>::underlyingType));
	}

	// Non-templated version for dynamic runtime types.
	int getAttributeFlags(const std::string &key, AttributeType type) const {
		return (sshsNodeGetAttributeFlags(node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type)));
	}

	template<AttributeType T> std::string getAttributeDescription(const std::string &key) const {
		char *desc = sshsNodeGetAttributeDescription(node, key.c_str(), AttributeTypeGenerator<T>::underlyingType);

		std::string retVal(desc);

		free(desc);

		return (retVal);
	}

	// Non-templated version for dynamic runtime types.
	std::string getAttributeDescription(const std::string &key, AttributeType type) const {
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

	bool existsRelativeNode(const std::string &nodePath) const {
		return (sshsExistsRelativeNode(node, nodePath.c_str()));
	}

	/**
	 * This returns a reference to a node, and as such must be carefully mediated with
	 * any sshsNodeRemoveNode() calls.
	 *
	 * @throws std::out_of_range Node not found.
	 */
	Node getRelativeNode(const std::string &nodePath) const {
		sshsNode node = sshsGetRelativeNode(node, nodePath.c_str());

		if (node == nullptr) {
			throw std::out_of_range("Node not found.");
		}

		return (node);
	}

	void attributeUpdaterAdd(
		const std::string &key, AttributeType type, sshsAttributeUpdater updater, void *updaterUserData) {
		sshsAttributeUpdaterAdd(
			node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), updater, updaterUserData);
	}

	void attributeUpdaterRemove(
		const std::string &key, AttributeType type, sshsAttributeUpdater updater, void *updaterUserData) {
		sshsAttributeUpdaterRemove(
			node, key.c_str(), static_cast<enum sshs_node_attr_value_type>(type), updater, updaterUserData);
	}

	void attributeUpdaterRemoveAllForNode() {
		sshsAttributeUpdaterRemoveAllForNode(node);
	}
};

class Helper {
public:
	static std::string typeToStringConverter(AttributeType type) {
		return (sshsHelperTypeToStringConverter(static_cast<enum sshs_node_attr_value_type>(type)));
	}

	static AttributeType stringToTypeConverter(const std::string &typeString) {
		return (static_cast<AttributeType>(sshsHelperStringToTypeConverter(typeString.c_str())));
	}

	static std::string valueToStringConverter(AttributeType type, union sshs_node_attr_value value) {
		char *val = sshsHelperValueToStringConverter(static_cast<enum sshs_node_attr_value_type>(type), value);

		std::string retVal(val);

		free(val);

		return (retVal);
	}

	static union sshs_node_attr_value stringToValueConverter(AttributeType type, const std::string &valueString) {
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

	static std::string rangesToStringConverter(AttributeType type, struct sshs_node_attr_ranges ranges) {
		char *val = sshsHelperRangesToStringConverter(static_cast<enum sshs_node_attr_value_type>(type), ranges);

		std::string retVal(val);

		free(val);

		return (retVal);
	}

	static struct sshs_node_attr_ranges stringToRangesConverter(AttributeType type, const std::string &rangesString) {
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

	explicit operator sshs() const {
		return (tree);
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

	bool existsNode(const std::string &nodePath) const {
		return (sshsExistsNode(tree, nodePath.c_str()));
	}

	Node getRootNode() const {
		return (sshsGetNode(tree, "/"));
	}

	/**
	 * This returns a reference to a node, and as such must be carefully mediated with
	 * any sshsNodeRemoveNode() calls.
	 *
	 * @throws std::out_of_range Node not found.
	 */
	Node getNode(const std::string &nodePath) const {
		sshsNode node = sshsGetNode(tree, nodePath.c_str());

		if (node == nullptr) {
			throw std::out_of_range("Node not found.");
		}

		return (node);
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

static Tree GLOBAL = Tree::getGlobal();

} // namespace Config
} // namespace dv

#endif /* DVCONFIG_HPP_ */
