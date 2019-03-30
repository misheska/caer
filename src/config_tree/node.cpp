#include "internal.hpp"

#include <algorithm>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/version.hpp>
#include <cfloat>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <utility>
#include <vector>

class dv_node_attribute {
private:
	dv_value value;
	dv_ranges ranges;
	int flags;
	std::string description;

public:
	dv_node_attribute() : flags(DVCFG_FLAGS_NORMAL) {
	}

	dv_node_attribute(
		const dv_value &_value, const dv_ranges &_ranges, int _flags, const std::string_view _description) :
		value(_value),
		ranges(_ranges),
		flags(_flags),
		description(_description) {
	}

	const dv_value &getValue() const noexcept {
		return (value);
	}

	const dv_ranges &getRanges() const noexcept {
		return (ranges);
	}

	int getFlags() const noexcept {
		return (flags);
	}

	bool isFlagSet(int flag) const noexcept {
		return ((flags & flag) == flag);
	}

	const std::string &getDescription() const noexcept {
		return (description);
	}
};

class dv_node_listener {
private:
	dvConfigNodeChangeListener nodeChanged;
	void *userData;

public:
	dv_node_listener(dvConfigNodeChangeListener _listener, void *_userData) :
		nodeChanged(_listener),
		userData(_userData) {
	}

	dvConfigNodeChangeListener getListener() const noexcept {
		return (nodeChanged);
	}

	void *getUserData() const noexcept {
		return (userData);
	}

	// Comparison operators.
	bool operator==(const dv_node_listener &rhs) const noexcept {
		return ((nodeChanged == rhs.nodeChanged) && (userData == rhs.userData));
	}

	bool operator!=(const dv_node_listener &rhs) const noexcept {
		return (!this->operator==(rhs));
	}
};

class dv_attribute_listener {
private:
	dvConfigAttributeChangeListener attributeChanged;
	void *userData;

public:
	dv_attribute_listener(dvConfigAttributeChangeListener _listener, void *_userData) :
		attributeChanged(_listener),
		userData(_userData) {
	}

	dvConfigAttributeChangeListener getListener() const noexcept {
		return (attributeChanged);
	}

	void *getUserData() const noexcept {
		return (userData);
	}

	// Comparison operators.
	bool operator==(const dv_attribute_listener &rhs) const noexcept {
		return ((attributeChanged == rhs.attributeChanged) && (userData == rhs.userData));
	}

	bool operator!=(const dv_attribute_listener &rhs) const noexcept {
		return (!this->operator==(rhs));
	}
};

static const std::regex dvKeyRegexp("^[a-zA-Z][a-zA-Z-_\\d\\.]*$");
static const std::regex dvModifierKeyRegexp("^_[a-zA-Z][a-zA-Z-_\\d\\.]*$");

// struct for C compatibility
struct dv_config_node {
public:
	std::string name;
	std::string path;
	dvConfigTree global;
	dvConfigNode parent;
	std::map<std::string, dvConfigNode> children;
	std::map<std::string, dv_node_attribute> attributes;
	std::vector<dv_node_listener> nodeListeners;
	std::vector<dv_attribute_listener> attrListeners;
	std::shared_mutex traversal_lock;
	std::recursive_mutex node_lock;

	dv_config_node(const std::string &_name, dvConfigNode _parent, dvConfigTree _global) :
		name(_name),
		global(_global),
		parent(_parent) {
		// Path is based on parent.
		if (_parent != nullptr) {
			path = parent->path + _name + "/";
		}
		else {
			// Or the root has an empty, constant path.
			path = "/";
		}
	}

	void createAttribute(const std::string &key, const dv_value &defaultValue, const dv_ranges &ranges, int flags,
		const std::string &description, bool isModifierKey) {
		// Check key name string against allowed characters via regexp.
		if (!std::regex_match(key, (isModifierKey ? dvModifierKeyRegexp : dvKeyRegexp))) {
			boost::format errorMsg = boost::format("Invalid key name format: '%s'.") % key;
			dvConfigNodeError("dvConfigNodeCreateAttribute", key, defaultValue.getType(), errorMsg.str());
		}

		if (ranges.min > ranges.max) {
			dvConfigNodeError("dvConfigNodeCreateAttribute", key, defaultValue.getType(),
				"minimum range cannot be bigger than maximum range.");
		}

		// Strings cannot be shorter than 0 characters (empty string).
		// Strings are special, their length range goes from 0 to SIZE_MAX, but we
		// have to restrict that to from 0 to INT32_MAX for languages like Java
		// that only support integer string lengths. It's also reasonable.
		if (defaultValue.getType() == DVCFG_TYPE_STRING) {
			if (std::get<int32_t>(ranges.min) < 0) {
				dvConfigNodeError(
					"dvConfigNodeCreateAttribute", key, DVCFG_TYPE_STRING, "minimum string range must be positive.");
			}
		}

		// Check that value conforms to range limits.
		if (!defaultValue.inRange(ranges)) {
			// Fail on wrong default value. Must be within range!
			boost::format errorMsg = boost::format("default value '%s' is out of specified range. "
												   "Please make sure the default value is within the given range!")
									 % dvConfigHelperCppValueToStringConverter(defaultValue);

			dvConfigNodeError("dvConfigNodeCreateAttribute", key, defaultValue.getType(), errorMsg.str());
		}

		enum dvConfigAttributeEvents attrEvents;

		std::scoped_lock lock(node_lock);

		// Add if not present, else replace.
		if (!attributes.count(key)) {
			// Insert newAttr. Execute Listener support.
			attributes[key] = dv_node_attribute(defaultValue, ranges, flags, description);
			attrEvents      = DVCFG_ATTRIBUTE_ADDED;
		}
		else {
			const auto &oldAttr = attributes[key];

			// To simplify things, we don't support multiple types per key.
			if (oldAttr.getValue().getType() != defaultValue.getType()) {
				boost::format errorMsg = boost::format("attribute already exists and has a different type of '%s'")
										 % dvConfigHelperCppTypeToStringConverter(oldAttr.getValue().getType());

				dvConfigNodeError("dvConfigNodeCreateAttribute", key, defaultValue.getType(), errorMsg.str());
			}

			// If old value is out of range, replace with new default value, which must be in
			// range and thus different. Else keep old value.
			bool valueChanged = false;
			if (!oldAttr.getValue().inRange(ranges)) {
				valueChanged = true;
			}

			bool extraChanged = false;
			if ((oldAttr.getFlags() != flags) || (oldAttr.getRanges() != ranges)
				|| (oldAttr.getDescription() != description)) {
				extraChanged = true;
			}

			// Always call listeners on modification in create. Flags, ranges, description
			// might have changed and we want to ensure that is notified.
			if (!valueChanged && !extraChanged) {
				// Nothing changed, maybe same call. Do nothing.
				return;
			}
			else if (!valueChanged && extraChanged) {
				// Only the extra bits changed, value was still fine.
				attributes[key] = dv_node_attribute(oldAttr.getValue(), ranges, flags, description);
				attrEvents      = DVCFG_ATTRIBUTE_MODIFIED_CREATE;
			}
			else if (valueChanged && !extraChanged) {
				// Only the value changed, same as a put(). Use new value.
				attributes[key] = dv_node_attribute(defaultValue, ranges, flags, description);
				attrEvents      = DVCFG_ATTRIBUTE_MODIFIED;
			}
			else { // valueChanged && extraChanged
				   // Everything changed. Use new value.
				attributes[key] = dv_node_attribute(defaultValue, ranges, flags, description);
				attrEvents      = DVCFG_ATTRIBUTE_MODIFIED_CREATE;
			}
		}

		// Listener support.
		const auto &listenerAttr = attributes[key];

		auto globalListener = dvConfigGlobalAttributeListenerGetFunction(this->global);
		if (globalListener != nullptr) {
			// Global listener support.
			(*globalListener)(this, dvConfigGlobalAttributeListenerGetUserData(this->global), attrEvents, key.c_str(),
				listenerAttr.getValue().getType(), listenerAttr.getValue().toCUnion(true));
		}

		for (const auto &l : attrListeners) {
			(*l.getListener())(this, l.getUserData(), attrEvents, key.c_str(), listenerAttr.getValue().getType(),
				listenerAttr.getValue().toCUnion(true));
		}
	}

	void createAttribute(const std::string &key, const dv_value &defaultValue, const dv_ranges &ranges, int flags,
		const std::string &description) {
		createAttribute(key, defaultValue, ranges, flags, description, false);
	}

	void removeAttribute(const std::string &key, enum dvConfigAttributeType type) {
		std::scoped_lock lock(node_lock);

		if (!attributeExists(key, type)) {
			// Ignore calls on non-existent attributes for remove, as it is used
			// to clean-up attributes before re-creating them in a consistent way.
			return;
		}

		const auto &attr = attributes[key];

		// Listener support.
		dvConfigAttributeChangeListener globalListener = dvConfigGlobalAttributeListenerGetFunction(this->global);
		if (globalListener != nullptr) {
			// Global listener support.
			(*globalListener)(this, dvConfigGlobalAttributeListenerGetUserData(this->global), DVCFG_ATTRIBUTE_REMOVED,
				key.c_str(), attr.getValue().getType(), attr.getValue().toCUnion(true));
		}

		for (const auto &l : attrListeners) {
			(*l.getListener())(this, l.getUserData(), DVCFG_ATTRIBUTE_REMOVED, key.c_str(), attr.getValue().getType(),
				attr.getValue().toCUnion(true));
		}

		// Remove attribute from node.
		attributes.erase(key);
	}

	void removeAllAttributes() {
		std::scoped_lock lock(node_lock);

		for (const auto &attr : attributes) {
			dvConfigAttributeChangeListener globalListener = dvConfigGlobalAttributeListenerGetFunction(this->global);
			if (globalListener != nullptr) {
				// Global listener support.
				(*globalListener)(this, dvConfigGlobalAttributeListenerGetUserData(this->global),
					DVCFG_ATTRIBUTE_REMOVED, attr.first.c_str(), attr.second.getValue().getType(),
					attr.second.getValue().toCUnion(true));
			}

			for (const auto &l : attrListeners) {
				(*l.getListener())(this, l.getUserData(), DVCFG_ATTRIBUTE_REMOVED, attr.first.c_str(),
					attr.second.getValue().getType(), attr.second.getValue().toCUnion(true));
			}
		}

		attributes.clear();
	}

	bool attributeExists(const std::string &key, enum dvConfigAttributeType type) {
		std::scoped_lock lockNode(node_lock);

		if ((!attributes.count(key)) || (attributes[key].getValue().getType() != type)) {
			errno = ENOENT;
			return (false);
		}

		// The specified attribute exists and has a matching type.
		return (true);
	}

	const dv_value getAttribute(const std::string &key, enum dvConfigAttributeType type) {
		std::scoped_lock lockNode(node_lock);

		if (!attributeExists(key, type)) {
			dvConfigNodeErrorNoAttribute("dvConfigNodeGetAttribute", key, type);
		}

		// Return a copy of the final value.
		return (attributes[key].getValue());
	}

	bool putAttribute(const std::string &key, const dv_value &value, bool forceReadOnlyUpdate = false) {
		std::scoped_lock lockNode(node_lock);

		if (!attributeExists(key, value.getType())) {
			dvConfigNodeErrorNoAttribute("dvConfigNodePutAttribute", key, value.getType());
		}

		const auto &attr = attributes[key];

		// Value must be present, so update old one, after checking range and flags.
		if ((!forceReadOnlyUpdate && attr.isFlagSet(DVCFG_FLAGS_READ_ONLY))
			|| (forceReadOnlyUpdate && !attr.isFlagSet(DVCFG_FLAGS_READ_ONLY))) {
			// Read-only flag set, cannot put new value!
			errno = EPERM;
			return (false);
		}

		if (!value.inRange(attr.getRanges())) {
			// New value out of range, cannot put new value!
			errno = ERANGE;
			return (false);
		}

		// Key and valueType have to be the same, so we first check that the
		// actual values, that we want to update, are different. If not, there's
		// nothing to do, no listeners to call, and it doesn't make sense to
		// set the value twice to the same content.
		if (attr.getValue() != value) {
			attributes[key] = dv_node_attribute(value, attr.getRanges(), attr.getFlags(), attr.getDescription());

			// Call the appropriate listeners, on change only, which is always
			// true at this point.
			dvConfigAttributeChangeListener globalListener = dvConfigGlobalAttributeListenerGetFunction(this->global);
			if (globalListener != nullptr) {
				// Global listener support.
				(*globalListener)(this, dvConfigGlobalAttributeListenerGetUserData(this->global),
					DVCFG_ATTRIBUTE_MODIFIED, key.c_str(), value.getType(), value.toCUnion(true));
			}

			for (const auto &l : attrListeners) {
				(*l.getListener())(this, l.getUserData(), DVCFG_ATTRIBUTE_MODIFIED, key.c_str(), value.getType(),
					value.toCUnion(true));
			}
		}

		return (true);
	}
};

static void dvConfigNodeDestroy(dvConfigNode node);
static void dvConfigNodeRemoveChild(dvConfigNode node, const std::string childName);
static void dvConfigNodeRemoveAllChildren(dvConfigNode node);

static void dvConfigNodeCreateString(dvConfigNode node, const char *key, const char *defaultValue, int32_t minLength,
	int32_t maxLength, int flags, const char *description, bool isModifierKey);

#define XML_INDENT_SPACES 4

static bool dvConfigNodeToXML(dvConfigNode node, int fd, bool recursive);
static boost::property_tree::ptree dvConfigNodeGenerateXML(dvConfigNode node, bool recursive);
static bool dvConfigNodeFromXML(dvConfigNode node, int fd, bool recursive, bool strict);
static void dvConfigNodeConsumeXML(dvConfigNode node, const boost::property_tree::ptree &content, bool recursive);

dvConfigNode dvConfigNodeNew(const char *nodeName, dvConfigNode parent, dvConfigTree global) {
	dvConfigNode newNode = new (std::nothrow) dv_config_node(nodeName, parent, global);
	dvConfigMemoryCheck(newNode, __func__);

	return (newNode);
}

// children, attributes, and listeners must be cleaned up prior to this call.
static void dvConfigNodeDestroy(dvConfigNode node) {
	delete node;
}

const char *dvConfigNodeGetName(dvConfigNode node) {
	return (node->name.c_str());
}

const char *dvConfigNodeGetPath(dvConfigNode node) {
	return (node->path.c_str());
}

dvConfigNode dvConfigNodeGetParent(dvConfigNode node) {
	return (node->parent);
}

dvConfigTree dvConfigNodeGetGlobal(dvConfigNode node) {
	return (node->global);
}

dvConfigNode dvConfigNodeAddChild(dvConfigNode node, const char *childName) {
	std::unique_lock<std::shared_mutex> lock(node->traversal_lock);

	if (node->children.count(childName)) {
		return (node->children[childName]);
	}
	else {
		// Create new child node with appropriate name and parent.
		dvConfigNode newChild = dvConfigNodeNew(childName, node, node->global);

		// No node present, let's add it.
		node->children[childName] = newChild;

		// Listener support (only on new addition!).
		std::scoped_lock nodeLock(node->node_lock);

		dvConfigNodeChangeListener globalListener = dvConfigGlobalNodeListenerGetFunction(node->global);
		if (globalListener != nullptr) {
			// Global listener support.
			(*globalListener)(
				node, dvConfigGlobalNodeListenerGetUserData(node->global), DVCFG_NODE_CHILD_ADDED, childName);
		}

		for (const auto &l : node->nodeListeners) {
			(*l.getListener())(node, l.getUserData(), DVCFG_NODE_CHILD_ADDED, childName);
		}

		return (newChild);
	}
}

dvConfigNode dvConfigNodeGetChild(dvConfigNode node, const char *childName) {
	std::shared_lock<std::shared_mutex> lock(node->traversal_lock);

	if (node->children.count(childName)) {
		return (node->children[childName]);
	}
	else {
		return (nullptr);
	}
}

dvConfigNode *dvConfigNodeGetChildren(dvConfigNode node, size_t *numChildren) {
	std::shared_lock<std::shared_mutex> lock(node->traversal_lock);

	size_t childrenCount = node->children.size();

	// If none, exit gracefully.
	if (childrenCount == 0) {
		*numChildren = 0;
		return (nullptr);
	}

	dvConfigNode *children = static_cast<dvConfigNode *>(malloc(childrenCount * sizeof(*children)));
	dvConfigMemoryCheck(children, __func__);

	size_t i = 0;
	for (const auto &n : node->children) {
		children[i++] = n.second;
	}

	*numChildren = childrenCount;
	return (children);
}

void dvConfigNodeAddNodeListener(dvConfigNode node, void *userData, dvConfigNodeChangeListener node_changed) {
	dv_node_listener listener(node_changed, userData);

	std::scoped_lock lock(node->node_lock);

	if (!findBool(node->nodeListeners.begin(), node->nodeListeners.end(), listener)) {
		node->nodeListeners.push_back(listener);
	}
}

void dvConfigNodeRemoveNodeListener(dvConfigNode node, void *userData, dvConfigNodeChangeListener node_changed) {
	dv_node_listener listener(node_changed, userData);

	std::scoped_lock lock(node->node_lock);

	node->nodeListeners.erase(
		std::remove(node->nodeListeners.begin(), node->nodeListeners.end(), listener), node->nodeListeners.end());
}

void dvConfigNodeRemoveAllNodeListeners(dvConfigNode node) {
	std::scoped_lock lock(node->node_lock);

	node->nodeListeners.clear();
}

void dvConfigNodeAddAttributeListener(
	dvConfigNode node, void *userData, dvConfigAttributeChangeListener attribute_changed) {
	dv_attribute_listener listener(attribute_changed, userData);

	std::scoped_lock lock(node->node_lock);

	if (!findBool(node->attrListeners.begin(), node->attrListeners.end(), listener)) {
		node->attrListeners.push_back(listener);
	}
}

void dvConfigNodeRemoveAttributeListener(
	dvConfigNode node, void *userData, dvConfigAttributeChangeListener attribute_changed) {
	dv_attribute_listener listener(attribute_changed, userData);

	std::scoped_lock lock(node->node_lock);

	node->attrListeners.erase(
		std::remove(node->attrListeners.begin(), node->attrListeners.end(), listener), node->attrListeners.end());
}

void dvConfigNodeRemoveAllAttributeListeners(dvConfigNode node) {
	std::scoped_lock lock(node->node_lock);

	node->attrListeners.clear();
}

void dvConfigNodeClearSubTree(dvConfigNode startNode, bool clearStartNode) {
	std::scoped_lock lockNode(startNode->node_lock);

	// Recurse down children and remove all attributes.
	size_t numChildren;
	dvConfigNode *children = dvConfigNodeGetChildren(startNode, &numChildren);

	for (size_t i = 0; i < numChildren; i++) {
		dvConfigNodeClearSubTree(children[i], true);
	}

	free(children);

	// Clear this node's attributes, if requested.
	if (clearStartNode) {
		dvConfigNodeRemoveAllAttributes(startNode);
		dvConfigNodeRemoveAllAttributeListeners(startNode);
	}
}

// Eliminates this node and any children. Nobody can have a reference, or
// be in the process of getting one, to this node or any of its children.
// You need to make sure of this in your application!
void dvConfigNodeRemoveNode(dvConfigNode node) {
	{
		std::scoped_lock lockNode(node->node_lock);

		// Now we can clear the subtree from all attribute related data.
		dvConfigNodeClearSubTree(node, true);

		// And finally remove the node related data and the node itself.
		dvConfigNodeRemoveSubTree(node);
	}

	// If this is the root node (parent == nullptr), it isn't fully removed.
	if (node->parent != nullptr) {
		// Unlink this node from the parent.
		// This also destroys the memory associated with the node.
		// Any later access is illegal!
		dvConfigNodeRemoveChild(node->parent, node->name);
	}
}

void dvConfigNodeRemoveSubTree(dvConfigNode node) {
	std::scoped_lock lockNode(node->node_lock);

	// Recurse down first, we remove from the bottom up.
	size_t numChildren;
	dvConfigNode *children = dvConfigNodeGetChildren(node, &numChildren);

	for (size_t i = 0; i < numChildren; i++) {
		dvConfigNodeRemoveSubTree(children[i]);
	}

	free(children);

	// Delete node listeners and children.
	dvConfigNodeRemoveAllChildren(node);
	dvConfigNodeRemoveAllNodeListeners(node);
}

// children, attributes, and listeners for the child to be removed
// must be cleaned up prior to this call.
static void dvConfigNodeRemoveChild(dvConfigNode node, const std::string childName) {
	std::unique_lock<std::shared_mutex> lock(node->traversal_lock);
	std::scoped_lock lockNode(node->node_lock);

	if (!node->children.count(childName)) {
		// Verify that a valid node exists, else simply return
		// without doing anything. Node was already deleted.
		return;
	}

	// Listener support.
	dvConfigNodeChangeListener globalListener = dvConfigGlobalNodeListenerGetFunction(node->global);
	if (globalListener != nullptr) {
		// Global listener support.
		(*globalListener)(
			node, dvConfigGlobalNodeListenerGetUserData(node->global), DVCFG_NODE_CHILD_REMOVED, childName.c_str());
	}

	for (const auto &l : node->nodeListeners) {
		(*l.getListener())(node, l.getUserData(), DVCFG_NODE_CHILD_REMOVED, childName.c_str());
	}

	dvConfigNodeDestroy(node->children[childName]);

	// Remove attribute from node.
	node->children.erase(childName);
}

// children, attributes, and listeners for the children to be removed
// must be cleaned up prior to this call.
static void dvConfigNodeRemoveAllChildren(dvConfigNode node) {
	std::unique_lock<std::shared_mutex> lock(node->traversal_lock);
	std::scoped_lock lockNode(node->node_lock);

	for (const auto &child : node->children) {
		dvConfigNodeChangeListener globalListener = dvConfigGlobalNodeListenerGetFunction(node->global);
		if (globalListener != nullptr) {
			// Global listener support.
			(*globalListener)(node, dvConfigGlobalNodeListenerGetUserData(node->global), DVCFG_NODE_CHILD_REMOVED,
				child.first.c_str());
		}

		for (const auto &l : node->nodeListeners) {
			(*l.getListener())(node, l.getUserData(), DVCFG_NODE_CHILD_REMOVED, child.first.c_str());
		}

		dvConfigNodeDestroy(child.second);
	}

	node->children.clear();
}

void dvConfigNodeCreateAttribute(dvConfigNode node, const char *key, enum dvConfigAttributeType type,
	union dvConfigAttributeValue defaultValue, const struct dvConfigAttributeRanges ranges, int flags,
	const char *description) {
	dv_value v;
	v.fromCUnion(defaultValue, type);

	dv_ranges r;
	r.fromCStruct(ranges, type);

	node->createAttribute(key, v, r, flags, description);
}

void dvConfigNodeRemoveAttribute(dvConfigNode node, const char *key, enum dvConfigAttributeType type) {
	node->removeAttribute(key, type);
}

void dvConfigNodeRemoveAllAttributes(dvConfigNode node) {
	node->removeAllAttributes();
}

bool dvConfigNodeExistsAttribute(dvConfigNode node, const char *key, enum dvConfigAttributeType type) {
	return (node->attributeExists(key, type));
}

bool dvConfigNodePutAttribute(
	dvConfigNode node, const char *key, enum dvConfigAttributeType type, union dvConfigAttributeValue value) {
	dv_value v;
	v.fromCUnion(value, type);

	return (node->putAttribute(key, v));
}

union dvConfigAttributeValue dvConfigNodeGetAttribute(
	dvConfigNode node, const char *key, enum dvConfigAttributeType type) {
	return (node->getAttribute(key, type).toCUnion());
}

bool dvConfigNodeUpdateReadOnlyAttribute(
	dvConfigNode node, const char *key, enum dvConfigAttributeType type, union dvConfigAttributeValue value) {
	dv_value v;
	v.fromCUnion(value, type);

	return (node->putAttribute(key, v, true));
}

void dvConfigNodeCreateBool(dvConfigNode node, const char *key, bool defaultValue, int flags, const char *description) {
	dv_value v;
	v.emplace<bool>(defaultValue);

	// No range for booleans.
	dv_ranges r;

	node->createAttribute(key, v, r, flags, description);
}

bool dvConfigNodePutBool(dvConfigNode node, const char *key, bool value) {
	dv_value v;
	v.emplace<bool>(value);

	return (node->putAttribute(key, v));
}

bool dvConfigNodeGetBool(dvConfigNode node, const char *key) {
	return (std::get<bool>(node->getAttribute(key, DVCFG_TYPE_BOOL)));
}

void dvConfigNodeCreateInt(dvConfigNode node, const char *key, int32_t defaultValue, int32_t minValue, int32_t maxValue,
	int flags, const char *description) {
	dv_value v;
	v.emplace<int32_t>(defaultValue);

	dv_ranges r;
	r.min.emplace<int32_t>(minValue);
	r.max.emplace<int32_t>(maxValue);

	node->createAttribute(key, v, r, flags, description);
}

bool dvConfigNodePutInt(dvConfigNode node, const char *key, int32_t value) {
	dv_value v;
	v.emplace<int32_t>(value);

	return (node->putAttribute(key, v));
}

int32_t dvConfigNodeGetInt(dvConfigNode node, const char *key) {
	return (std::get<int32_t>(node->getAttribute(key, DVCFG_TYPE_INT)));
}

void dvConfigNodeCreateLong(dvConfigNode node, const char *key, int64_t defaultValue, int64_t minValue,
	int64_t maxValue, int flags, const char *description) {
	dv_value v;
	v.emplace<int64_t>(defaultValue);

	dv_ranges r;
	r.min.emplace<int64_t>(minValue);
	r.max.emplace<int64_t>(maxValue);

	node->createAttribute(key, v, r, flags, description);
}

bool dvConfigNodePutLong(dvConfigNode node, const char *key, int64_t value) {
	dv_value v;
	v.emplace<int64_t>(value);

	return (node->putAttribute(key, v));
}

int64_t dvConfigNodeGetLong(dvConfigNode node, const char *key) {
	return (std::get<int64_t>(node->getAttribute(key, DVCFG_TYPE_LONG)));
}

void dvConfigNodeCreateFloat(dvConfigNode node, const char *key, float defaultValue, float minValue, float maxValue,
	int flags, const char *description) {
	dv_value v;
	v.emplace<float>(defaultValue);

	dv_ranges r;
	r.min.emplace<float>(minValue);
	r.max.emplace<float>(maxValue);

	node->createAttribute(key, v, r, flags, description);
}

bool dvConfigNodePutFloat(dvConfigNode node, const char *key, float value) {
	dv_value v;
	v.emplace<float>(value);

	return (node->putAttribute(key, v));
}

float dvConfigNodeGetFloat(dvConfigNode node, const char *key) {
	return (std::get<float>(node->getAttribute(key, DVCFG_TYPE_FLOAT)));
}

void dvConfigNodeCreateDouble(dvConfigNode node, const char *key, double defaultValue, double minValue, double maxValue,
	int flags, const char *description) {
	dv_value v;
	v.emplace<double>(defaultValue);

	dv_ranges r;
	r.min.emplace<double>(minValue);
	r.max.emplace<double>(maxValue);

	node->createAttribute(key, v, r, flags, description);
}

bool dvConfigNodePutDouble(dvConfigNode node, const char *key, double value) {
	dv_value v;
	v.emplace<double>(value);

	return (node->putAttribute(key, v));
}

double dvConfigNodeGetDouble(dvConfigNode node, const char *key) {
	return (std::get<double>(node->getAttribute(key, DVCFG_TYPE_DOUBLE)));
}

static void dvConfigNodeCreateString(dvConfigNode node, const char *key, const char *defaultValue, int32_t minLength,
	int32_t maxLength, int flags, const char *description, bool isModifierKey) {
	dv_value v;
	v.emplace<std::string>(defaultValue);

	dv_ranges r;
	r.min.emplace<int32_t>(minLength);
	r.max.emplace<int32_t>(maxLength);

	node->createAttribute(key, v, r, flags, description, isModifierKey);
}

void dvConfigNodeCreateString(dvConfigNode node, const char *key, const char *defaultValue, int32_t minLength,
	int32_t maxLength, int flags, const char *description) {
	dvConfigNodeCreateString(node, key, defaultValue, minLength, maxLength, flags, description, false);
}

bool dvConfigNodePutString(dvConfigNode node, const char *key, const char *value) {
	dv_value v;
	v.emplace<std::string>(value);

	return (node->putAttribute(key, v));
}

// This is a copy of the string on the heap, remember to free() when done!
char *dvConfigNodeGetString(dvConfigNode node, const char *key) {
	return (node->getAttribute(key, DVCFG_TYPE_STRING).toCUnion().string);
}

bool dvConfigNodeExportNodeToXML(dvConfigNode node, int fd) {
	return (dvConfigNodeToXML(node, fd, false));
}

bool dvConfigNodeExportSubTreeToXML(dvConfigNode node, int fd) {
	return (dvConfigNodeToXML(node, fd, true));
}

static bool dvConfigNodeToXML(dvConfigNode node, int fd, bool recursive) {
	boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_sink> fdStream(
		fd, boost::iostreams::never_close_handle);

	std::ostream outStream(&fdStream);

	boost::property_tree::ptree xmlTree;

	// Add main configuration tree node and version.
	xmlTree.put("dv.<xmlattr>.version", "2.0");

	// Generate recursive XML for all nodes.
	xmlTree.put_child("dv.node", dvConfigNodeGenerateXML(node, recursive));

	try {
#if defined(BOOST_VERSION) && (BOOST_VERSION / 100000) == 1 && (BOOST_VERSION / 100 % 1000) < 56
		boost::property_tree::xml_parser::xml_writer_settings<boost::property_tree::ptree::key_type::value_type>
			xmlIndent(' ', XML_INDENT_SPACES);
#else
		boost::property_tree::xml_parser::xml_writer_settings<boost::property_tree::ptree::key_type> xmlIndent(
			' ', XML_INDENT_SPACES);
#endif

		// We manually call write_xml_element() here instead of write_xml() because
		// the latter always prepends the XML declaration, which we don't want.
		boost::property_tree::xml_parser::write_xml_element(
			outStream, boost::property_tree::ptree::key_type(), xmlTree, -1, xmlIndent);
		if (!outStream) {
			throw boost::property_tree::xml_parser_error("write error.", std::string(), 0);
		}
	}
	catch (const boost::property_tree::xml_parser_error &ex) {
		const std::string errorMsg = std::string("Failed to write XML to output stream. Exception: ") + ex.what();
		(*dvConfigTreeErrorLogCallbackGet())(errorMsg.c_str(), false);
		return (false);
	}

	return (true);
}

static boost::property_tree::ptree dvConfigNodeGenerateXML(dvConfigNode node, bool recursive) {
	boost::property_tree::ptree content;

	// First recurse down all the way to the leaf children, where attributes are kept.
	if (recursive) {
		std::shared_lock<std::shared_mutex> lock(node->traversal_lock);

		for (const auto &child : node->children) {
			auto childContent = dvConfigNodeGenerateXML(child.second, recursive);

			if (!childContent.empty()) {
				// Only add in nodes that have content (attributes or other nodes).
				content.add_child("node", childContent);
			}
		}
	}

	std::scoped_lock lockNode(node->node_lock);

	// Then it's attributes (key:value pairs).
	auto attrFirstIterator = content.begin();
	for (const auto &attr : node->attributes) {
		// If an attribute is marked NO_EXPORT or IMPORTED, we skip it.
		if (attr.second.isFlagSet(DVCFG_FLAGS_NO_EXPORT) || attr.second.isFlagSet(DVCFG_FLAGS_IMPORTED)) {
			continue;
		}

		const std::string type  = dvConfigHelperCppTypeToStringConverter(attr.second.getValue().getType());
		const std::string value = dvConfigHelperCppValueToStringConverter(attr.second.getValue());

		boost::property_tree::ptree attrNode(value);
		attrNode.put("<xmlattr>.key", attr.first);
		attrNode.put("<xmlattr>.type", type);

		// Attributes should be in order, but at the start of the node (before
		// other nodes), so we insert() them instead of just adding to the back.
		attrFirstIterator
			= content.insert(attrFirstIterator, boost::property_tree::ptree::value_type("attr", attrNode));
		attrFirstIterator++;
	}

	if (!content.empty()) {
		// Only add elements (name, path) if the node has any content
		// (attributes or othern odes), so that empty nodes are really empty.
		content.put("<xmlattr>.name", node->name);
		content.put("<xmlattr>.path", node->path);
	}

	return (content);
}

bool dvConfigNodeImportNodeFromXML(dvConfigNode node, int fd, bool strict) {
	return (dvConfigNodeFromXML(node, fd, false, strict));
}

bool dvConfigNodeImportSubTreeFromXML(dvConfigNode node, int fd, bool strict) {
	return (dvConfigNodeFromXML(node, fd, true, strict));
}

static std::vector<std::reference_wrapper<const boost::property_tree::ptree>> dvConfigNodeXMLFilterChildNodes(
	const boost::property_tree::ptree &content, const std::string &name) {
	std::vector<std::reference_wrapper<const boost::property_tree::ptree>> result;

	for (const auto &elem : content) {
		if (elem.first == name) {
			result.push_back(elem.second);
		}
	}

	return (result);
}

static bool dvConfigNodeFromXML(dvConfigNode node, int fd, bool recursive, bool strict) {
	boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source> fdStream(
		fd, boost::iostreams::never_close_handle);

	std::istream inStream(&fdStream);

	boost::property_tree::ptree xmlTree;

	try {
		boost::property_tree::xml_parser::read_xml(
			inStream, xmlTree, boost::property_tree::xml_parser::trim_whitespace);
	}
	catch (const boost::property_tree::xml_parser_error &ex) {
		const std::string errorMsg = std::string("Failed to load XML from input stream. Exception: ") + ex.what();
		(*dvConfigTreeErrorLogCallbackGet())(errorMsg.c_str(), false);
		return (false);
	}

	// Check name and version for compliance.
	try {
		const auto dvConfigVersion = xmlTree.get<std::string>("dv.<xmlattr>.version");
		if (dvConfigVersion != "2.0") {
			throw boost::property_tree::ptree_error("unsupported configuration tree version (supported: '2.0').");
		}
	}
	catch (const boost::property_tree::ptree_error &ex) {
		const std::string errorMsg = std::string("Invalid XML content. Exception: ") + ex.what();
		(*dvConfigTreeErrorLogCallbackGet())(errorMsg.c_str(), false);
		return (false);
	}

	auto root = dvConfigNodeXMLFilterChildNodes(xmlTree.get_child("dv"), "node");

	if (root.size() != 1) {
		(*dvConfigTreeErrorLogCallbackGet())("Multiple or no root child nodes present.", false);
		return (false);
	}

	auto &rootNode = root.front().get();

	// Strict mode: check if names match.
	if (strict) {
		try {
			const auto rootNodeName = rootNode.get<std::string>("<xmlattr>.name");

			if (rootNodeName != node->name) {
				throw boost::property_tree::ptree_error("names don't match (required in 'strict' mode).");
			}
		}
		catch (const boost::property_tree::ptree_error &ex) {
			const std::string errorMsg = std::string("Invalid root node. Exception: ") + ex.what();
			(*dvConfigTreeErrorLogCallbackGet())(errorMsg.c_str(), false);
			return (false);
		}
	}

	dvConfigNodeConsumeXML(node, rootNode, recursive);

	return (true);
}

static void dvConfigNodeConsumeXML(dvConfigNode node, const boost::property_tree::ptree &content, bool recursive) {
	auto attributes = dvConfigNodeXMLFilterChildNodes(content, "attr");

	for (auto &attr : attributes) {
		// Check that the proper attributes exist.
		const auto key  = attr.get().get("<xmlattr>.key", "");
		const auto type = attr.get().get("<xmlattr>.type", "");

		if (key.empty() || type.empty()) {
			continue;
		}

		// Get the needed values.
		const auto value = attr.get().get_value("");

		if (!dvConfigNodeStringToAttributeConverter(node, key.c_str(), type.c_str(), value.c_str(), true)) {
			// Ignore read-only/range errors.
			if (errno == EPERM || errno == ERANGE) {
				continue;
			}

			boost::format errorMsg
				= boost::format("failed to convert attribute from XML, value string was '%s'") % value;

			dvConfigNodeError(
				"dvConfigNodeConsumeXML", key, dvConfigHelperCppStringToTypeConverter(type), errorMsg.str(), false);
		}
	}

	if (recursive) {
		auto children = dvConfigNodeXMLFilterChildNodes(content, "node");

		for (auto &child : children) {
			// Check that the proper attributes exist.
			const auto childName = child.get().get("<xmlattr>.name", "");

			if (childName.empty()) {
				continue;
			}

			// Get the child node.
			dvConfigNode childNode = dvConfigNodeGetChild(node, childName.c_str());

			// If not existing, try to create.
			if (childNode == nullptr) {
				childNode = dvConfigNodeAddChild(node, childName.c_str());
			}

			// And call recursively.
			dvConfigNodeConsumeXML(childNode, child.get(), recursive);
		}
	}
}

// For more precise failure reason, look at errno.
bool dvConfigNodeStringToAttributeConverter(
	dvConfigNode node, const char *key, const char *typeStr, const char *valueStr, bool overrideReadOnly) {
	// Parse the values according to type and put them in the node.
	enum dvConfigAttributeType type;
	type = dvConfigHelperCppStringToTypeConverter(typeStr);

	if (type == DVCFG_TYPE_UNKNOWN) {
		errno = EINVAL;
		return (false);
	}

	if ((type == DVCFG_TYPE_STRING) && (valueStr == nullptr)) {
		// Empty string.
		valueStr = "";
	}

	dv_value value;
	try {
		value = dvConfigHelperCppStringToValueConverter(type, valueStr);
	}
	catch (const std::invalid_argument &) {
		errno = EINVAL;
		return (false);
	}
	catch (const std::out_of_range &) {
		errno = EINVAL;
		return (false);
	}

	{
		std::scoped_lock lockNode(node->node_lock);

		// IFF attribute already exists, we update it using dvConfigNodePut(), else
		// we create the attribute with maximum range and a default description.
		// These XML-loaded attributes are marked as IMPORTED and READ_ONLY, until
		// another call to createAttribute() sets the flags properly for usage.
		// This happens on XML load only. More restrictive ranges and flags can be
		// enabled later by calling dvConfigNodeCreate*() again as needed.
		bool result = false;

		if (node->attributeExists(key, type)) {
			result = node->putAttribute(
				key, value, (overrideReadOnly && node->attributes[key].isFlagSet(DVCFG_FLAGS_READ_ONLY)));
		}
		else {
			// Create never fails, it may exit the program, but not fail!
			result = true;
			dv_ranges ranges;
			bool isModifierKey = key[0] == '_';

			switch (type) {
				case DVCFG_TYPE_BOOL:
					// No ranges for bool.
					node->createAttribute(key, value, ranges, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_IMPORTED,
						"XML loaded value.", isModifierKey);
					break;

				case DVCFG_TYPE_INT:
					ranges.min.emplace<int32_t>(INT32_MIN);
					ranges.max.emplace<int32_t>(INT32_MAX);
					node->createAttribute(key, value, ranges, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_IMPORTED,
						"XML loaded value.", isModifierKey);
					break;

				case DVCFG_TYPE_LONG:
					ranges.min.emplace<int64_t>(INT64_MIN);
					ranges.max.emplace<int64_t>(INT64_MAX);
					node->createAttribute(key, value, ranges, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_IMPORTED,
						"XML loaded value.", isModifierKey);
					break;

				case DVCFG_TYPE_FLOAT:
					ranges.min.emplace<float>(-FLT_MAX);
					ranges.max.emplace<float>(FLT_MAX);
					node->createAttribute(key, value, ranges, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_IMPORTED,
						"XML loaded value.", isModifierKey);
					break;

				case DVCFG_TYPE_DOUBLE:
					ranges.min.emplace<double>(-DBL_MAX);
					ranges.max.emplace<double>(DBL_MAX);
					node->createAttribute(key, value, ranges, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_IMPORTED,
						"XML loaded value.", isModifierKey);
					break;

				case DVCFG_TYPE_STRING:
					ranges.min.emplace<int32_t>(0);
					ranges.max.emplace<int32_t>(INT32_MAX);
					node->createAttribute(key, value, ranges, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_IMPORTED,
						"XML loaded value.", isModifierKey);
					break;

				case DVCFG_TYPE_UNKNOWN:
					errno  = EINVAL;
					result = false;
					break;
			}
		}

		return (result);
	}
}

// Remember to free the resulting array.
const char **dvConfigNodeGetChildNames(dvConfigNode node, size_t *numNames) {
	std::shared_lock<std::shared_mutex> lock(node->traversal_lock);

	if (node->children.empty()) {
		*numNames = 0;
		errno     = ENOENT;
		return (nullptr);
	}

	size_t numChildren = node->children.size();

	// Nodes can be deleted, so we copy the string's contents into
	// memory that will be guaranteed to exist.
	size_t childNamesLength = 0;

	for (const auto &child : node->children) {
		// Length plus one for terminating NUL byte.
		childNamesLength += child.first.length() + 1;
	}

	char **childNames = static_cast<char **>(malloc((numChildren * sizeof(char *)) + childNamesLength));
	dvConfigMemoryCheck(childNames, __func__);

	size_t offset = (numChildren * sizeof(char *));

	size_t i = 0;
	for (const auto &child : node->children) {
		// We have all the memory, so now copy the strings over and set the
		// pointers as if an array of pointers was the only result.
		childNames[i] = reinterpret_cast<char *>(reinterpret_cast<uint8_t *>(childNames) + offset);
		strcpy(childNames[i], child.first.c_str());

		// Length plus one for terminating NUL byte.
		offset += child.first.length() + 1;
		i++;
	}

	*numNames = numChildren;
	return (const_cast<const char **>(childNames));
}

// Remember to free the resulting array.
const char **dvConfigNodeGetAttributeKeys(dvConfigNode node, size_t *numKeys) {
	std::scoped_lock lockNode(node->node_lock);

	if (node->attributes.empty()) {
		*numKeys = 0;
		errno    = ENOENT;
		return (nullptr);
	}

	size_t numAttributes = node->attributes.size();

	// Attributes can be deleted, so we copy the key string's contents into
	// memory that will be guaranteed to exist.
	size_t attributeKeysLength = 0;

	for (const auto &attr : node->attributes) {
		// Length plus one for terminating NUL byte.
		attributeKeysLength += attr.first.length() + 1;
	}

	char **attributeKeys = static_cast<char **>(malloc((numAttributes * sizeof(char *)) + attributeKeysLength));
	dvConfigMemoryCheck(attributeKeys, __func__);

	size_t offset = (numAttributes * sizeof(char *));

	size_t i = 0;
	for (const auto &attr : node->attributes) {
		// We have all the memory, so now copy the strings over and set the
		// pointers as if an array of pointers was the only result.
		attributeKeys[i] = reinterpret_cast<char *>(reinterpret_cast<uint8_t *>(attributeKeys) + offset);
		strcpy(attributeKeys[i], attr.first.c_str());

		// Length plus one for terminating NUL byte.
		offset += attr.first.length() + 1;
		i++;
	}

	*numKeys = numAttributes;
	return (const_cast<const char **>(attributeKeys));
}

enum dvConfigAttributeType dvConfigNodeGetAttributeType(dvConfigNode node, const char *key) {
	std::scoped_lock lockNode(node->node_lock);

	if (!node->attributes.count(key)) {
		errno = ENOENT;
		return (DVCFG_TYPE_UNKNOWN);
	}

	// There is exactly one type for one specific attribute key.
	return (node->attributes[key].getValue().getType());
}

struct dvConfigAttributeRanges dvConfigNodeGetAttributeRanges(
	dvConfigNode node, const char *key, enum dvConfigAttributeType type) {
	std::scoped_lock lockNode(node->node_lock);

	if (!node->attributeExists(key, type)) {
		dvConfigNodeErrorNoAttribute("dvConfigNodeGetAttributeRanges", key, type);
	}

	return (node->attributes[key].getRanges().toCStruct());
}

int dvConfigNodeGetAttributeFlags(dvConfigNode node, const char *key, enum dvConfigAttributeType type) {
	std::scoped_lock lockNode(node->node_lock);

	if (!node->attributeExists(key, type)) {
		dvConfigNodeErrorNoAttribute("dvConfigNodeGetAttributeFlags", key, type);
	}

	return (node->attributes[key].getFlags());
}

// Remember to free the resulting string.
char *dvConfigNodeGetAttributeDescription(dvConfigNode node, const char *key, enum dvConfigAttributeType type) {
	std::scoped_lock lockNode(node->node_lock);

	if (!node->attributeExists(key, type)) {
		dvConfigNodeErrorNoAttribute("dvConfigNodeGetAttributeDescription", key, type);
	}

	char *descriptionCopy = strdup(node->attributes[key].getDescription().c_str());
	dvConfigMemoryCheck(descriptionCopy, __func__);

	return (descriptionCopy);
}

void dvConfigNodeAttributeModifierButton(dvConfigNode node, const char *key, const char *type) {
	std::scoped_lock lockNode(node->node_lock);

	if (!node->attributeExists(key, DVCFG_TYPE_BOOL)) {
		dvConfigNodeErrorNoAttribute("dvConfigNodeAttributeModifierButton", key, DVCFG_TYPE_BOOL);
	}

	std::string typeStr(type);
	if (!typeStr.empty() && typeStr != "PLAY" && typeStr != "ONOFF" && typeStr != "EXECUTE") {
		dvConfigNodeError("dvConfigNodeAttributeModifierButton", key, DVCFG_TYPE_BOOL,
			"Unknown Button type; permitted are: <empty>, PLAY, ONOFF, EXECUTE.");
	}

	std::string fullKey(key);
	fullKey = "_" + fullKey + "Button";

	dvConfigNodeCreateString(node, fullKey.c_str(), type, 0, INT32_MAX, DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT,
		"Type of button to display (PLAY, EXECUTE, ...; can be empty).", true);
}

void dvConfigNodeAttributeModifierListOptions(
	dvConfigNode node, const char *key, const char *listOptions, bool allowMultipleSelections) {
	std::scoped_lock lockNode(node->node_lock);

	if (!node->attributeExists(key, DVCFG_TYPE_STRING)) {
		dvConfigNodeErrorNoAttribute("dvConfigNodeAttributeModifierListOptions", key, DVCFG_TYPE_STRING);
	}

	std::string options(listOptions);
	if (options.empty()) {
		dvConfigNodeError(
			"dvConfigNodeAttributeModifierListOptions", key, DVCFG_TYPE_STRING, "List options cannot be empty.");
	}

	std::string fullKey(key);
	fullKey = "_" + fullKey + "ListOptions";

	if (allowMultipleSelections) {
		fullKey += "Multi";
	}

	dvConfigNodeCreateString(node, fullKey.c_str(), listOptions, 1, INT32_MAX, DVCFG_FLAGS_READ_ONLY,
		"Comma separated list of possible choices for attribute value.", true);
}

void dvConfigNodeAttributeModifierFileChooser(dvConfigNode node, const char *key, const char *typeAndExtensions) {
	std::scoped_lock lockNode(node->node_lock);

	if (!node->attributeExists(key, DVCFG_TYPE_STRING)) {
		dvConfigNodeErrorNoAttribute("dvConfigNodeAttributeModifierFileChooser", key, DVCFG_TYPE_STRING);
	}

	std::string typeExt(typeAndExtensions);
	auto sepPos = typeExt.find(':');

	std::string typeStr;
	if (sepPos == std::string::npos) {
		// No colon, so only type.
		typeStr = typeExt;
	}
	else {
		// Colon, so first part is type.
		typeStr = typeExt.substr(0, sepPos);
	}

	if (typeStr != "DIRECTORY" && typeStr != "LOAD" && typeStr != "SAVE") {
		dvConfigNodeError("dvConfigNodeAttributeModifierFileChooser", key, DVCFG_TYPE_STRING,
			"Unknown FileChooser type; permitted are: DIRECTORY, LOAD, SAVE.");
	}

	std::string fullKey(key);
	fullKey = "_" + fullKey + "FileChooser";

	dvConfigNodeCreateString(node, fullKey.c_str(), typeAndExtensions, 1, INT32_MAX,
		DVCFG_FLAGS_READ_ONLY | DVCFG_FLAGS_NO_EXPORT,
		"Type of file chooser dialog plus optional comma separated list of allowed extensions.", true);
}

void dvConfigNodeAttributeModifierUnit(dvConfigNode node, const char *key, const char *unitInformation) {
	std::scoped_lock lockNode(node->node_lock);

	if (!node->attributeExists(key, DVCFG_TYPE_INT) && !node->attributeExists(key, DVCFG_TYPE_LONG)
		&& !node->attributeExists(key, DVCFG_TYPE_FLOAT) && !node->attributeExists(key, DVCFG_TYPE_DOUBLE)) {
		dvConfigNodeErrorNoAttribute("dvConfigNodeAttributeModifierUnit", key, DVCFG_TYPE_INT);
	}

	std::string unitInfo(unitInformation);
	if (unitInfo.empty()) {
		dvConfigNodeError(
			"dvConfigNodeAttributeModifierUnit", key, DVCFG_TYPE_INT, "Unit information cannot be empty.");
	}

	std::string fullKey(key);
	fullKey = "_" + fullKey + "Unit";

	dvConfigNodeCreateString(node, fullKey.c_str(), unitInformation, 1, INT32_MAX, DVCFG_FLAGS_READ_ONLY,
		"Information about the units that apply to a numeric attribute (ms, Km, m, Kg, mg, ...).", true);
}

void dvConfigNodeAttributeModifierPriorityAttributes(dvConfigNode node, const char *priorityAttributes) {
	std::scoped_lock lockNode(node->node_lock);

	dvConfigNodeCreateString(node, "_priorityAttributes", priorityAttributes, 0, INT32_MAX, DVCFG_FLAGS_NORMAL,
		"Comma separated list of attributes to prioritize regarding visualization in the UI (can be empty).", true);
}

void dvConfigNodeAttributeButtonReset(dvConfigNode node, const char *key) {
	dvConfigNodeAttributeUpdaterAdd(node, key, DVCFG_TYPE_BOOL,
		[](void *, const char *, enum dvConfigAttributeType) {
			dvConfigAttributeValue val;
			val.boolean = false;
			return (val);
		},
		nullptr, true);
}
