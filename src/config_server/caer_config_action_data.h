#ifndef SRC_CONFIG_SERVER_CAER_CONFIG_ACTION_DATA_H_
#define SRC_CONFIG_SERVER_CAER_CONFIG_ACTION_DATA_H_

#include "caer-sdk/utils.h"

#include <array>

enum class caerConfigAction {
	NODE_EXISTS        = 0,
	ATTR_EXISTS        = 1,
	GET                = 2,
	PUT                = 3,
	ERROR              = 4,
	GET_CHILDREN       = 5,
	GET_ATTRIBUTES     = 6,
	GET_TYPE           = 7,
	GET_RANGES         = 8,
	GET_FLAGS          = 9,
	GET_DESCRIPTION    = 10,
	ADD_MODULE         = 11,
	REMOVE_MODULE      = 12,
	ADD_PUSH_CLIENT    = 13,
	REMOVE_PUSH_CLIENT = 14,
	PUSH_MESSAGE_NODE  = 15,
	PUSH_MESSAGE_ATTR  = 16,
	DUMP_TREE          = 17,
	DUMP_TREE_NODE     = 18,
	DUMP_TREE_ATTR     = 19,
};

// Control message format: 1 byte ACTION, 1 byte TYPE, 2 bytes EXTRA_LEN,
// 2 bytes NODE_LEN, 2 bytes KEY_LEN, 2 bytes VALUE_LEN, then up to 4086
// bytes split between EXTRA, NODE, KEY, VALUE (with 4 bytes for NUL).
// Basically: (EXTRA_LEN + NODE_LEN + KEY_LEN + VALUE_LEN) <= 4086.
// EXTRA, NODE, KEY, VALUE have to be NUL terminated, and their length
// must include the NUL termination byte.
// This results in a maximum message size of 4096 bytes (4KB).
// All two-byte integers (EXTRA_LEN etc.) are little-endian!
#define CAER_CONFIG_SERVER_BUFFER_SIZE 4096
#define CAER_CONFIG_SERVER_HEADER_SIZE 10

class caerConfigActionData {
private:
	std::array<uint8_t, CAER_CONFIG_SERVER_BUFFER_SIZE> buffer;

public:
	caerConfigActionData() noexcept {
		reset();
	}

	void setAction(caerConfigAction action) {
		buffer[0] = static_cast<uint8_t>(action);
	}

	caerConfigAction getAction() const {
		return (static_cast<caerConfigAction>(buffer[0]));
	}

	void setType(dv::Config::AttributeType type) {
		buffer[1] = static_cast<uint8_t>(type);
	}

	dv::Config::AttributeType getType() const {
		return (static_cast<dv::Config::AttributeType>(buffer[1]));
	}

	void setExtraLength(uint16_t extraLen) {
		*(reinterpret_cast<uint16_t *>(&buffer[2])) = htole16(extraLen);
	}

	uint16_t getExtraLength() const {
		return (le16toh(*(reinterpret_cast<const uint16_t *>(&buffer[2]))));
	}

	void setNodeLength(uint16_t nodeLen) {
		*(reinterpret_cast<uint16_t *>(&buffer[4])) = htole16(nodeLen);
	}

	uint16_t getNodeLength() const {
		return (le16toh(*(reinterpret_cast<const uint16_t *>(&buffer[4]))));
	}

	void setKeyLength(uint16_t keyLen) {
		*(reinterpret_cast<uint16_t *>(&buffer[6])) = htole16(keyLen);
	}

	uint16_t getKeyLength() const {
		return (le16toh(*(reinterpret_cast<const uint16_t *>(&buffer[6]))));
	}

	void setValueLength(uint16_t valueLen) {
		*(reinterpret_cast<uint16_t *>(&buffer[8])) = htole16(valueLen);
	}

	uint16_t getValueLength() const {
		return (le16toh(*(reinterpret_cast<const uint16_t *>(&buffer[8]))));
	}

	void setExtra(const std::string &extra) {
		memcpy(&buffer[CAER_CONFIG_SERVER_HEADER_SIZE], extra.c_str(), extra.length());
		buffer[CAER_CONFIG_SERVER_HEADER_SIZE + extra.length()] = '\0';
		setExtraLength(static_cast<uint16_t>(extra.length() + 1)); // +1 for NUL termination.
	}

	std::string getExtra() const {
		if (getExtraLength() < 2) {
			return (std::string());
		}

		// Construct with size to avoid useless strlen() pass; -1 to drop terminating NUL char.
		return (
			std::string(reinterpret_cast<const char *>(&buffer[CAER_CONFIG_SERVER_HEADER_SIZE]), getExtraLength() - 1));
	}

	void setNode(const std::string &node) {
		memcpy(&buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength()], node.c_str(), node.length());
		buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + node.length()] = '\0';
		setNodeLength(static_cast<uint16_t>(node.length() + 1)); // +1 for NUL termination.
	}

	std::string getNode() const {
		if (getNodeLength() < 2) {
			return (std::string());
		}

		// Construct with size to avoid useless strlen() pass; -1 to drop terminating NUL char.
		return (std::string(reinterpret_cast<const char *>(&buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength()]),
			getNodeLength() - 1));
	}

	void setKey(const std::string &key) {
		memcpy(&buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength()], key.c_str(), key.length());
		buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength() + key.length()] = '\0';
		setKeyLength(static_cast<uint16_t>(key.length() + 1)); // +1 for NUL termination.
	}

	std::string getKey() const {
		if (getKeyLength() < 2) {
			return (std::string());
		}

		// Construct with size to avoid useless strlen() pass; -1 to drop terminating NUL char.
		return (std::string(reinterpret_cast<const char *>(
								&buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength()]),
			getKeyLength() - 1));
	}

	void setValue(const std::string &value) {
		memcpy(&buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength() + getKeyLength()],
			value.c_str(), value.length());
		buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength() + getKeyLength() + value.length()]
			= '\0';
		setValueLength(static_cast<uint16_t>(value.length() + 1)); // +1 for NUL termination.
	}

	std::string getValue() const {
		if (getValueLength() < 2) {
			return (std::string());
		}

		// Construct with size to avoid useless strlen() pass; -1 to drop terminating NUL char.
		return (std::string(
			reinterpret_cast<const char *>(
				&buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength() + getKeyLength()]),
			getValueLength() - 1));
	}

	void reset() {
		buffer.fill(0);
	}

	uint8_t *getBuffer() {
		return (getHeaderBuffer());
	}

	const uint8_t *getBuffer() const {
		return (getHeaderBuffer());
	}

	size_t size() const {
		return (headerSize() + dataSize());
	}

	uint8_t *getHeaderBuffer() {
		return (&buffer[0]);
	}

	const uint8_t *getHeaderBuffer() const {
		return (&buffer[0]);
	}

	size_t headerSize() const {
		return (CAER_CONFIG_SERVER_HEADER_SIZE);
	}

	uint8_t *getDataBuffer() {
		return (&buffer[CAER_CONFIG_SERVER_HEADER_SIZE]);
	}

	const uint8_t *getDataBuffer() const {
		return (&buffer[CAER_CONFIG_SERVER_HEADER_SIZE]);
	}

	size_t dataSize() const {
		return (getExtraLength() + getNodeLength() + getKeyLength() + getValueLength());
	}

	std::string toString() const {
		return ("action=" + std::to_string(static_cast<uint8_t>(getAction())) + ", type="
				+ std::to_string(static_cast<uint8_t>(getType())) + ", extraLength=" + std::to_string(getExtraLength())
				+ ", nodeLength=" + std::to_string(getNodeLength()) + ", keyLength=" + std::to_string(getKeyLength())
				+ ", valueLength=" + std::to_string(getValueLength()));
	}
};

#endif /* SRC_CONFIG_SERVER_CAER_CONFIG_ACTION_DATA_H_ */
