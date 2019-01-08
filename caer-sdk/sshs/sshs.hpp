#ifndef SSHS_HPP_
#define SSHS_HPP_

#include "sshs.h"

#include <string>

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
