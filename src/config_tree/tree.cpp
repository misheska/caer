#include "internal.hpp"

#include <atomic>
#include <boost/tokenizer.hpp>
#include <iostream>
#include <mutex>
#include <regex>
#include <vector>

class sshs_attribute_updater {
private:
	dvConfigNode node;
	std::string key;
	enum dvConfigAttributeType type;
	dvConfigAttributeUpdater updater;
	void *userData;

public:
	sshs_attribute_updater(dvConfigNode _node, const std::string &_key, enum dvConfigAttributeType _type,
		dvConfigAttributeUpdater _updater, void *_userData) :
		node(_node),
		key(_key),
		type(_type),
		updater(_updater),
		userData(_userData) {
	}

	dvConfigNode getNode() const noexcept {
		return (node);
	}

	const std::string &getKey() const noexcept {
		return (key);
	}

	enum dvConfigAttributeType getType() const noexcept {
		return (type);
	}

	dvConfigAttributeUpdater getUpdater() const noexcept {
		return (updater);
	}

	void *getUserData() const noexcept {
		return (userData);
	}

	// Comparison operators.
	bool operator==(const sshs_attribute_updater &rhs) const noexcept {
		return ((node == rhs.node) && (key == rhs.key) && (type == rhs.type) && (updater == rhs.updater)
				&& (userData == rhs.userData));
	}

	bool operator!=(const sshs_attribute_updater &rhs) const noexcept {
		return (!this->operator==(rhs));
	}
};

// struct for C compatibility
struct dv_config_tree {
public:
	// Data root node. Cannot be deleted.
	dvConfigNode root;
	// Global attribute updaters.
	std::vector<sshs_attribute_updater> attributeUpdaters;
	std::mutex attributeUpdatersLock;
	// Global node listener.
	std::atomic<dvConfigNodeChangeListener> globalNodeListenerFunction;
	std::atomic<void *> globalNodeListenerUserData;
	// Global attribute listener.
	std::atomic<dvConfigAttributeChangeListener> globalAttributeListenerFunction;
	std::atomic<void *> globalAttributeListenerUserData;
	// Lock to serialize setting of global listeners.
	std::mutex globalListenersLock;
};

static void sshsGlobalInitialize(void);
static void sshsGlobalErrorLogCallbackInitialize(void);
static void sshsGlobalErrorLogCallbackSetInternal(dvConfigTreeErrorLogCallback error_log_cb);
static void sshsDefaultErrorLogCallback(const char *msg, bool fatal);
static bool sshsCheckAbsoluteNodePath(const std::string &absolutePath);
static bool sshsCheckRelativeNodePath(const std::string &relativePath);

static dvConfigTree sshsGlobal = nullptr;
static std::once_flag sshsGlobalIsInitialized;

static void sshsGlobalInitialize(void) {
	sshsGlobal = dvConfigTreeNew();
}

dvConfigTree dvConfigTreeGlobal(void) {
	std::call_once(sshsGlobalIsInitialized, &sshsGlobalInitialize);

	return (sshsGlobal);
}

static dvConfigTreeErrorLogCallback sshsGlobalErrorLogCallback = nullptr;
static std::once_flag sshsGlobalErrorLogCallbackIsInitialized;

static void sshsGlobalErrorLogCallbackInitialize(void) {
	sshsGlobalErrorLogCallbackSetInternal(&sshsDefaultErrorLogCallback);
}

static void sshsGlobalErrorLogCallbackSetInternal(dvConfigTreeErrorLogCallback error_log_cb) {
	sshsGlobalErrorLogCallback = error_log_cb;
}

dvConfigTreeErrorLogCallback dvConfigTreeErrorLogCallbackGet(void) {
	std::call_once(sshsGlobalErrorLogCallbackIsInitialized, &sshsGlobalErrorLogCallbackInitialize);

	return (sshsGlobalErrorLogCallback);
}

/**
 * This is not thread-safe, and it's not meant to be.
 * Set the global error callback preferably only once, before using SSHS.
 */
void dvConfigTreeErrorLogCallbackSet(dvConfigTreeErrorLogCallback error_log_cb) {
	std::call_once(sshsGlobalErrorLogCallbackIsInitialized, &sshsGlobalErrorLogCallbackInitialize);

	// If nullptr, set to default logging callback.
	if (error_log_cb == nullptr) {
		sshsGlobalErrorLogCallbackSetInternal(&sshsDefaultErrorLogCallback);
	}
	else {
		sshsGlobalErrorLogCallbackSetInternal(error_log_cb);
	}
}

dvConfigTree dvConfigTreeNew(void) {
	dvConfigTree newSshs = (dvConfigTree) malloc(sizeof(*newSshs));
	sshsMemoryCheck(newSshs, __func__);

	// Create root node.
	newSshs->root = sshsNodeNew("", nullptr, newSshs);

	// Initialize C++ objects using placement new.
	new (&newSshs->attributeUpdaters) std::vector<sshs_attribute_updater>();
	new (&newSshs->attributeUpdatersLock) std::mutex();
	new (&newSshs->globalNodeListenerFunction) std::atomic<dvConfigNodeChangeListener>(nullptr);
	new (&newSshs->globalNodeListenerUserData) std::atomic<void *>(nullptr);
	new (&newSshs->globalAttributeListenerFunction) std::atomic<dvConfigAttributeChangeListener>(nullptr);
	new (&newSshs->globalAttributeListenerUserData) std::atomic<void *>(nullptr);
	new (&newSshs->globalListenersLock) std::mutex();

	return (newSshs);
}

bool dvConfigTreeExistsNode(dvConfigTree st, const char *nodePathC) {
	const std::string nodePath(nodePathC);

	if (!sshsCheckAbsoluteNodePath(nodePath)) {
		errno = EINVAL;
		return (false);
	}

	// First node is the root.
	dvConfigNode curr = st->root;

	// Optimization: the root node always exists.
	if (nodePath == "/") {
		return (true);
	}

	boost::tokenizer<boost::char_separator<char>> nodePathTokens(nodePath, boost::char_separator<char>("/"));

	// Search (or create) viable node iteratively.
	for (const auto &tok : nodePathTokens) {
		dvConfigNode next = sshsNodeGetChild(curr, tok.c_str());

		// If node doesn't exist, return that.
		if (next == nullptr) {
			errno = ENOENT;
			return (false);
		}

		curr = next;
	}

	// We got to the end, so the node exists.
	return (true);
}

dvConfigNode dvConfigTreeGetNode(dvConfigTree st, const char *nodePathC) {
	const std::string nodePath(nodePathC);

	if (!sshsCheckAbsoluteNodePath(nodePath)) {
		errno = EINVAL;
		return (nullptr);
	}

	// First node is the root.
	dvConfigNode curr = st->root;

	// Optimization: the root node always exists and is right there.
	if (nodePath == "/") {
		return (curr);
	}

	boost::tokenizer<boost::char_separator<char>> nodePathTokens(nodePath, boost::char_separator<char>("/"));

	// Search (or create) viable node iteratively.
	for (const auto &tok : nodePathTokens) {
		dvConfigNode next = sshsNodeGetChild(curr, tok.c_str());

		// Create next node in path if not existing.
		if (next == nullptr) {
			next = sshsNodeAddChild(curr, tok.c_str());
		}

		curr = next;
	}

	// 'curr' now contains the specified node.
	return (curr);
}

bool sshsNodeExistsRelativeNode(dvConfigNode node, const char *nodePathC) {
	const std::string nodePath(nodePathC);

	if (!sshsCheckRelativeNodePath(nodePath)) {
		errno = EINVAL;
		return (false);
	}

	// Start with the given node.
	dvConfigNode curr = node;

	boost::tokenizer<boost::char_separator<char>> nodePathTokens(nodePath, boost::char_separator<char>("/"));

	// Search (or create) viable node iteratively.
	for (const auto &tok : nodePathTokens) {
		dvConfigNode next = sshsNodeGetChild(curr, tok.c_str());

		// If node doesn't exist, return that.
		if (next == nullptr) {
			errno = ENOENT;
			return (false);
		}

		curr = next;
	}

	// We got to the end, so the node exists.
	return (true);
}

dvConfigNode sshsNodeGetRelativeNode(dvConfigNode node, const char *nodePathC) {
	const std::string nodePath(nodePathC);

	if (!sshsCheckRelativeNodePath(nodePath)) {
		errno = EINVAL;
		return (nullptr);
	}

	// Start with the given node.
	dvConfigNode curr = node;

	boost::tokenizer<boost::char_separator<char>> nodePathTokens(nodePath, boost::char_separator<char>("/"));

	// Search (or create) viable node iteratively.
	for (const auto &tok : nodePathTokens) {
		dvConfigNode next = sshsNodeGetChild(curr, tok.c_str());

		// Create next node in path if not existing.
		if (next == nullptr) {
			next = sshsNodeAddChild(curr, tok.c_str());
		}

		curr = next;
	}

	// 'curr' now contains the specified node.
	return (curr);
}

void dvConfigNodeAttributeUpdaterAdd(dvConfigNode node, const char *key, enum dvConfigAttributeType type,
	dvConfigAttributeUpdater updater, void *updaterUserData) {
	sshs_attribute_updater attrUpdater(node, key, type, updater, updaterUserData);

	dvConfigTree tree = sshsNodeGetGlobal(node);
	std::lock_guard<std::mutex> lock(tree->attributeUpdatersLock);

	// Check no other updater already exists that matches this one.
	if (!findBool(tree->attributeUpdaters.begin(), tree->attributeUpdaters.end(), attrUpdater)) {
		// Verify referenced attribute actually exists.
		if (!sshsNodeExistsAttribute(node, key, type)) {
			sshsNodeErrorNoAttribute("sshsAttributeUpdaterAdd", key, type);
		}

		tree->attributeUpdaters.push_back(attrUpdater);
	}
}

void dvConfigNodeAttributeUpdaterRemove(dvConfigNode node, const char *key, enum dvConfigAttributeType type,
	dvConfigAttributeUpdater updater, void *updaterUserData) {
	sshs_attribute_updater attrUpdater(node, key, type, updater, updaterUserData);

	dvConfigTree tree = sshsNodeGetGlobal(node);
	std::lock_guard<std::mutex> lock(tree->attributeUpdatersLock);

	tree->attributeUpdaters.erase(
		std::remove(tree->attributeUpdaters.begin(), tree->attributeUpdaters.end(), attrUpdater),
		tree->attributeUpdaters.end());
}

void dvConfigNodeAttributeUpdaterRemoveAll(dvConfigNode node) {
	dvConfigTree tree = sshsNodeGetGlobal(node);
	std::lock_guard<std::mutex> lock(tree->attributeUpdatersLock);

	tree->attributeUpdaters.erase(std::remove_if(tree->attributeUpdaters.begin(), tree->attributeUpdaters.end(),
									  [&node](const sshs_attribute_updater &up) { return (up.getNode() == node); }),
		tree->attributeUpdaters.end());
}

void dvConfigTreeAttributeUpdaterRemoveAll(dvConfigTree tree) {
	std::lock_guard<std::mutex> lock(tree->attributeUpdatersLock);

	tree->attributeUpdaters.clear();
}

bool dvConfigTreeAttributeUpdaterRun(dvConfigTree tree) {
	std::lock_guard<std::mutex> lock(tree->attributeUpdatersLock);

	bool allSuccess = true;

	for (const auto &up : tree->attributeUpdaters) {
		union dvConfigAttributeValue newValue = (*up.getUpdater())(up.getUserData(), up.getKey().c_str(), up.getType());

		if (!sshsNodePutAttribute(up.getNode(), up.getKey().c_str(), up.getType(), newValue)) {
			allSuccess = false;
		}
	}

	return (allSuccess);
}

void dvConfigTreeGlobalNodeListenerSet(dvConfigTree tree, dvConfigNodeChangeListener node_changed, void *userData) {
	std::lock_guard<std::mutex> lock(tree->globalListenersLock);

	// Ensure function is never called with old user data.
	tree->globalNodeListenerUserData.store(nullptr);

	// Update function.
	tree->globalNodeListenerFunction.store(node_changed);

	// Associate new user data.
	tree->globalNodeListenerUserData.store(userData);
}

dvConfigNodeChangeListener sshsGlobalNodeListenerGetFunction(dvConfigTree tree) {
	return (tree->globalNodeListenerFunction.load(std::memory_order_relaxed));
}

void *sshsGlobalNodeListenerGetUserData(dvConfigTree tree) {
	return (tree->globalNodeListenerUserData.load(std::memory_order_relaxed));
}

void dvConfigTreeGlobalAttributeListenerSet(
	dvConfigTree tree, dvConfigAttributeChangeListener attribute_changed, void *userData) {
	std::lock_guard<std::mutex> lock(tree->globalListenersLock);

	// Ensure function is never called with old user data.
	tree->globalAttributeListenerUserData.store(nullptr);

	// Update function.
	tree->globalAttributeListenerFunction.store(attribute_changed);

	// Associate new user data.
	tree->globalAttributeListenerUserData.store(userData);
}

dvConfigAttributeChangeListener sshsGlobalAttributeListenerGetFunction(dvConfigTree tree) {
	return (tree->globalAttributeListenerFunction.load(std::memory_order_relaxed));
}

void *sshsGlobalAttributeListenerGetUserData(dvConfigTree tree) {
	return (tree->globalAttributeListenerUserData.load(std::memory_order_relaxed));
}

#define ALLOWED_CHARS_REGEXP "([a-zA-Z-_\\d\\.]+/)"
static const std::regex sshsAbsoluteNodePathRegexp("^/" ALLOWED_CHARS_REGEXP "*$");
static const std::regex sshsRelativeNodePathRegexp("^" ALLOWED_CHARS_REGEXP "+$");

static bool sshsCheckAbsoluteNodePath(const std::string &absolutePath) {
	if (absolutePath.empty()) {
		(*dvConfigTreeErrorLogCallbackGet())("Absolute node path cannot be empty.", false);
		return (false);
	}

	if (!std::regex_match(absolutePath, sshsAbsoluteNodePathRegexp)) {
		boost::format errorMsg = boost::format("Invalid absolute node path format: '%s'.") % absolutePath;

		(*dvConfigTreeErrorLogCallbackGet())(errorMsg.str().c_str(), false);

		return (false);
	}

	return (true);
}

static bool sshsCheckRelativeNodePath(const std::string &relativePath) {
	if (relativePath.empty()) {
		(*dvConfigTreeErrorLogCallbackGet())("Relative node path cannot be empty.", false);
		return (false);
	}

	if (!std::regex_match(relativePath, sshsRelativeNodePathRegexp)) {
		boost::format errorMsg = boost::format("Invalid relative node path format: '%s'.") % relativePath;

		(*dvConfigTreeErrorLogCallbackGet())(errorMsg.str().c_str(), false);

		return (false);
	}

	return (true);
}

static void sshsDefaultErrorLogCallback(const char *msg, bool fatal) {
	std::cerr << msg << std::endl;

	if (fatal) {
		exit(EXIT_FAILURE);
	}
}
