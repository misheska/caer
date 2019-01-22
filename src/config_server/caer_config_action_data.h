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

// Control message format: 1 byte ACTION, 1 byte TYPE, 8 bytes ID,
// 2 bytes EXTRA_LEN, 2 bytes NODE_LEN, 2 bytes KEY_LEN, 2 bytes
// VALUE_LEN, then up to 4078 bytes split between EXTRA, NODE, KEY
// and VALUE.
// Basically: (EXTRA_LEN + NODE_LEN + KEY_LEN + VALUE_LEN) <= 4078.
// EXTRA, NODE, KEY, VALUE shall not be NUL terminated, because
// the length is known already.
// This results in a maximum message size of 4096 bytes (4KB).
// All integers (EXTRA_LEN etc.) are little-endian!
#define CAER_CONFIG_SERVER_BUFFER_SIZE 4096
#define CAER_CONFIG_SERVER_HEADER_SIZE 18

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

	void setID(uint64_t id) {
		*(reinterpret_cast<uint64_t *>(&buffer[2])) = htole64(id);
	}

	uint64_t getID() const {
		return (le64toh(*(reinterpret_cast<const uint64_t *>(&buffer[2]))));
	}

	void setExtraLength(uint16_t extraLen) {
		*(reinterpret_cast<uint16_t *>(&buffer[10])) = htole16(extraLen);
	}

	uint16_t getExtraLength() const {
		return (le16toh(*(reinterpret_cast<const uint16_t *>(&buffer[10]))));
	}

	void setNodeLength(uint16_t nodeLen) {
		*(reinterpret_cast<uint16_t *>(&buffer[12])) = htole16(nodeLen);
	}

	uint16_t getNodeLength() const {
		return (le16toh(*(reinterpret_cast<const uint16_t *>(&buffer[12]))));
	}

	void setKeyLength(uint16_t keyLen) {
		*(reinterpret_cast<uint16_t *>(&buffer[14])) = htole16(keyLen);
	}

	uint16_t getKeyLength() const {
		return (le16toh(*(reinterpret_cast<const uint16_t *>(&buffer[14]))));
	}

	void setValueLength(uint16_t valueLen) {
		*(reinterpret_cast<uint16_t *>(&buffer[16])) = htole16(valueLen);
	}

	uint16_t getValueLength() const {
		return (le16toh(*(reinterpret_cast<const uint16_t *>(&buffer[16]))));
	}

	void setExtra(const std::string &extra) {
		memcpy(&buffer[CAER_CONFIG_SERVER_HEADER_SIZE], extra.data(), extra.size());
		setExtraLength(static_cast<uint16_t>(extra.size()));
	}

	std::string getExtra() const {
		if (getExtraLength() == 0) {
			return (std::string());
		}

		// Construct with size to avoid useless strlen() pass, size is known.
		return (std::string(reinterpret_cast<const char *>(&buffer[CAER_CONFIG_SERVER_HEADER_SIZE]), getExtraLength()));
	}

	void setNode(const std::string &node) {
		memcpy(&buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength()], node.data(), node.size());
		setNodeLength(static_cast<uint16_t>(node.size()));
	}

	std::string getNode() const {
		if (getNodeLength() == 0) {
			return (std::string());
		}

		// Construct with size to avoid useless strlen() pass, size is known.
		return (std::string(reinterpret_cast<const char *>(&buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength()]),
			getNodeLength()));
	}

	void setKey(const std::string &key) {
		memcpy(&buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength()], key.data(), key.size());
		setKeyLength(static_cast<uint16_t>(key.size()));
	}

	std::string getKey() const {
		if (getKeyLength() == 0) {
			return (std::string());
		}

		// Construct with size to avoid useless strlen() pass, size is known.
		return (std::string(reinterpret_cast<const char *>(
								&buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength()]),
			getKeyLength()));
	}

	void setValue(const std::string &value) {
		memcpy(&buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength() + getKeyLength()],
			value.data(), value.size());
		setValueLength(static_cast<uint16_t>(value.size()));
	}

	std::string getValue() const {
		if (getValueLength() == 0) {
			return (std::string());
		}

		// Construct with size to avoid useless strlen() pass, size is known.
		return (std::string(
			reinterpret_cast<const char *>(
				&buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength() + getKeyLength()]),
			getValueLength()));
	}

	void reset() {
		std::fill_n(buffer.begin(), CAER_CONFIG_SERVER_HEADER_SIZE, 0);
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
				+ std::to_string(static_cast<uint8_t>(getType())) + ", id=" + std::to_string(getID()) + ", extraLength="
				+ std::to_string(getExtraLength()) + ", nodeLength=" + std::to_string(getNodeLength()) + ", keyLength="
				+ std::to_string(getKeyLength()) + ", valueLength=" + std::to_string(getValueLength()));
	}
};

#endif /* SRC_CONFIG_SERVER_CAER_CONFIG_ACTION_DATA_H_ */
