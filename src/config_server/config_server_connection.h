#ifndef SRC_CONFIG_SERVER_CONFIG_SERVER_CONNECTION_H_
#define SRC_CONFIG_SERVER_CONFIG_SERVER_CONNECTION_H_

#include "asio.h"
#include "config_server_actions.h"
#include "config_server_main.h"

#include <memory>

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

class ConfigActionData {
private:
	uint8_t buffer[CAER_CONFIG_SERVER_BUFFER_SIZE];

public:
	ConfigActionData() {
	}

	void setAction(caer_config_actions action) {
		buffer[0] = static_cast<uint8_t>(action);
	}

	caer_config_actions getAction() const {
		return (static_cast<caer_config_actions>(buffer[0]));
	}

	void setType(sshs_node_attr_value_type type) {
		buffer[1] = static_cast<uint8_t>(type);
	}

	sshs_node_attr_value_type getType() const {
		return (static_cast<sshs_node_attr_value_type>(buffer[1]));
	}

	void setExtraLength(uint16_t extraLen) {
		*((uint16_t *) (buffer + 2)) = htole16(extraLen);
	}

	uint16_t getExtraLength() const {
		return (le16toh(*((const uint16_t *) (buffer + 2))));
	}

	void setNodeLength(uint16_t nodeLen) {
		*((uint16_t *) (buffer + 4)) = htole16(nodeLen);
	}

	uint16_t getNodeLength() const {
		return (le16toh(*((const uint16_t *) (buffer + 4))));
	}

	void setKeyLength(uint16_t keyLen) {
		*((uint16_t *) (buffer + 6)) = htole16(keyLen);
	}

	uint16_t getKeyLength() const {
		return (le16toh(*((const uint16_t *) (buffer + 6))));
	}

	void setValueLength(uint16_t valueLen) {
		*((uint16_t *) (buffer + 8)) = htole16(valueLen);
	}

	uint16_t getValueLength() const {
		return (le16toh(*((const uint16_t *) (buffer + 8))));
	}

	void setExtra(const std::string &extra) {
		memcpy(buffer + CAER_CONFIG_SERVER_HEADER_SIZE, extra.c_str(), extra.length());
		buffer[CAER_CONFIG_SERVER_HEADER_SIZE + extra.length()] = '\0';
		setExtraLength(static_cast<uint16_t>(extra.length() + 1)); // +1 for NUL termination.
	}

	std::string getExtra() const {
		if (getExtraLength() < 2) {
			return (std::string());
		}

		return (std::string(reinterpret_cast<const char *>(buffer + CAER_CONFIG_SERVER_HEADER_SIZE)));
	}

	void setNode(const std::string &node) {
		memcpy(buffer + CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength(), node.c_str(), node.length());
		buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + node.length()] = '\0';
		setNodeLength(static_cast<uint16_t>(node.length() + 1)); // +1 for NUL termination.
	}

	std::string getNode() const {
		if (getNodeLength() < 2) {
			return (std::string());
		}

		return (
			std::string(reinterpret_cast<const char *>(buffer + CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength())));
	}

	void setKey(const std::string &key) {
		memcpy(buffer + CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength(), key.c_str(), key.length());
		buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength() + key.length()] = '\0';
		setKeyLength(static_cast<uint16_t>(key.length() + 1)); // +1 for NUL termination.
	}

	std::string getKey() const {
		if (getKeyLength() < 2) {
			return (std::string());
		}

		return (std::string(reinterpret_cast<const char *>(
			buffer + CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength())));
	}

	void setValue(const std::string &value) {
		memcpy(buffer + CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength() + getKeyLength(),
			value.c_str(), value.length());
		buffer[CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength() + getKeyLength() + value.length()]
			= '\0';
		setValueLength(static_cast<uint16_t>(value.length() + 1)); // +1 for NUL termination.
	}

	std::string getValue() const {
		if (getValueLength() < 2) {
			return (std::string());
		}

		return (std::string(reinterpret_cast<const char *>(
			buffer + CAER_CONFIG_SERVER_HEADER_SIZE + getExtraLength() + getNodeLength() + getKeyLength())));
	}

	void reset() {
		memset(buffer, 0, CAER_CONFIG_SERVER_HEADER_SIZE);
	}

	uint8_t *getBuffer() {
		return (getHeaderBuffer());
	}

	size_t size() const {
		return (headerSize() + dataSize());
	}

	uint8_t *getHeaderBuffer() {
		return (buffer);
	}

	size_t headerSize() const {
		return (CAER_CONFIG_SERVER_HEADER_SIZE);
	}

	uint8_t *getDataBuffer() {
		return (buffer + CAER_CONFIG_SERVER_HEADER_SIZE);
	}

	size_t dataSize() const {
		return (getExtraLength() + getNodeLength() + getKeyLength() + getValueLength());
	}
};

// The response from the server follows a simplified version of the request
// protocol. A byte for ACTION, a byte for TYPE, 2 bytes for MSG_LEN and then
// up to 4092 bytes of MSG, for a maximum total of 4096 bytes again.
// MSG must be NUL terminated, and the NUL byte shall be part of the length.

class ConfigServer;

class ConfigServerConnection : public std::enable_shared_from_this<ConfigServerConnection> {
private:
	ConfigServer *parent;
	TCPTLSSocket socket;
	ConfigActionData data;

public:
	ConfigServerConnection(asioTCP::socket s, bool sslEnabled, asioSSL::context &sslContext, ConfigServer *server);
	~ConfigServerConnection();

	void start();
	void close();

	void addPushClient();
	void removePushClient();

	ConfigActionData &getData();
	void writeResponse();

private:
	void readHeader();
	void readData();
	void handleError(const boost::system::error_code &error, const char *message);
};

#endif /* SRC_CONFIG_SERVER_CONFIG_SERVER_CONNECTION_H_ */
