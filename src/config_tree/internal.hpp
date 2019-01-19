#ifndef CONFIG_TREE_INTERNAL_HPP_
#define CONFIG_TREE_INTERNAL_HPP_

// Implementation relevant common includes.
#include "caer-sdk/config/dvConfig.h"

#include <boost/format.hpp>
#include <cstring>
#include <stdexcept>
#include <string>

// C linkage to guarantee no name mangling.
extern "C" {
// Internal node functions.
dvConfigNode sshsNodeNew(const char *nodeName, dvConfigNode parent, dvConfigTree global);
/**
 * This returns a reference to a node, and as such must be carefully mediated with
 * any sshsNodeRemoveNode() calls.
 */
dvConfigNode sshsNodeAddChild(dvConfigNode node, const char *childName);
/**
 * This returns a reference to a node, and as such must be carefully mediated with
 * any sshsNodeRemoveNode() calls.
 */
dvConfigNode sshsNodeGetChild(dvConfigNode node, const char *childName);
/**
 * Get link to global SSHS tree.
 */
dvConfigTree sshsNodeGetGlobal(dvConfigNode node);

// Internal global functions.
dvConfigNodeChangeListener sshsGlobalNodeListenerGetFunction(dvConfigTree tree);
void *sshsGlobalNodeListenerGetUserData(dvConfigTree tree);
dvConfigAttributeChangeListener sshsGlobalAttributeListenerGetFunction(dvConfigTree tree);
void *sshsGlobalAttributeListenerGetUserData(dvConfigTree tree);
}

template<typename InIter, typename Elem> static inline bool findBool(InIter begin, InIter end, const Elem &val) {
	const auto result = std::find(begin, end, val);

	if (result == end) {
		return (false);
	}

	return (true);
}

// Terminate process on failed memory allocation.
template<typename T> static inline void sshsMemoryCheck(T *ptr, const std::string &funcName) {
	if (ptr == nullptr) {
		boost::format errorMsg = boost::format("%s(): unable to allocate memory.") % funcName;

		(*dvConfigTreeErrorLogCallbackGet())(errorMsg.str().c_str(), true);
	}
}

class sshs_value {
private:
	enum dvConfigAttributeType type;
	union {
		bool boolean;
		int32_t iint;
		int64_t ilong;
		float ffloat;
		double ddouble;
	} value;
	std::string valueString; // Separate for easy memory management.

public:
	sshs_value() {
		type        = DVCFG_TYPE_UNKNOWN;
		value.ilong = 0;
	}

	enum dvConfigAttributeType getType() const noexcept {
		return (type);
	}

	bool getBool() const {
		if (type != DVCFG_TYPE_BOOL) {
			throw std::runtime_error("SSHS: value type does not match requested type.");
		}

		return (value.boolean);
	}

	void setBool(bool v) noexcept {
		type          = DVCFG_TYPE_BOOL;
		value.boolean = v;
	}

	int32_t getInt() const {
		if (type != DVCFG_TYPE_INT) {
			throw std::runtime_error("SSHS: value type does not match requested type.");
		}

		return (value.iint);
	}

	void setInt(int32_t v) noexcept {
		type       = DVCFG_TYPE_INT;
		value.iint = v;
	}

	int64_t getLong() const {
		if (type != DVCFG_TYPE_LONG) {
			throw std::runtime_error("SSHS: value type does not match requested type.");
		}

		return (value.ilong);
	}

	void setLong(int64_t v) noexcept {
		type        = DVCFG_TYPE_LONG;
		value.ilong = v;
	}

	float getFloat() const {
		if (type != DVCFG_TYPE_FLOAT) {
			throw std::runtime_error("SSHS: value type does not match requested type.");
		}

		return (value.ffloat);
	}

	void setFloat(float v) noexcept {
		type         = DVCFG_TYPE_FLOAT;
		value.ffloat = v;
	}

	double getDouble() const {
		if (type != DVCFG_TYPE_DOUBLE) {
			throw std::runtime_error("SSHS: value type does not match requested type.");
		}

		return (value.ddouble);
	}

	void setDouble(double v) noexcept {
		type          = DVCFG_TYPE_DOUBLE;
		value.ddouble = v;
	}

	const std::string &getString() const {
		if (type != DVCFG_TYPE_STRING) {
			throw std::runtime_error("SSHS: value type does not match requested type.");
		}

		return (valueString);
	}

	void setString(const std::string &v) noexcept {
		type        = DVCFG_TYPE_STRING;
		valueString = v;
	}

	bool inRange(const struct dvConfigAttributeRanges &ranges) const {
		switch (type) {
			case DVCFG_TYPE_BOOL:
				// No check for bool, because no range exists.
				return (true);

			case DVCFG_TYPE_INT:
				return (value.iint >= ranges.min.iintRange && value.iint <= ranges.max.iintRange);

			case DVCFG_TYPE_LONG:
				return (value.ilong >= ranges.min.ilongRange && value.ilong <= ranges.max.ilongRange);

			case DVCFG_TYPE_FLOAT:
				return (value.ffloat >= ranges.min.ffloatRange && value.ffloat <= ranges.max.ffloatRange);

			case DVCFG_TYPE_DOUBLE:
				return (value.ddouble >= ranges.min.ddoubleRange && value.ddouble <= ranges.max.ddoubleRange);

			case DVCFG_TYPE_STRING:
				return (
					valueString.length() >= ranges.min.stringRange && valueString.length() <= ranges.max.stringRange);

			case DVCFG_TYPE_UNKNOWN:
			default:
				return (false);
		}
	}

	void fromCUnion(union dvConfigAttributeValue vu, enum dvConfigAttributeType tu) {
		switch (tu) {
			case DVCFG_TYPE_BOOL:
				setBool(vu.boolean);
				break;

			case DVCFG_TYPE_INT:
				setInt(vu.iint);
				break;

			case DVCFG_TYPE_LONG:
				setLong(vu.ilong);
				break;

			case DVCFG_TYPE_FLOAT:
				setFloat(vu.ffloat);
				break;

			case DVCFG_TYPE_DOUBLE:
				setDouble(vu.ddouble);
				break;

			case DVCFG_TYPE_STRING:
				setString(vu.string);
				break;

			case DVCFG_TYPE_UNKNOWN:
			default:
				throw std::runtime_error("SSHS: provided union value type does not match any valid type.");
				break;
		}
	}

	union dvConfigAttributeValue toCUnion(bool readOnlyString = false) const {
		union dvConfigAttributeValue vu;

		switch (type) {
			case DVCFG_TYPE_BOOL:
				vu.boolean = getBool();
				break;

			case DVCFG_TYPE_INT:
				vu.iint = getInt();
				break;

			case DVCFG_TYPE_LONG:
				vu.ilong = getLong();
				break;

			case DVCFG_TYPE_FLOAT:
				vu.ffloat = getFloat();
				break;

			case DVCFG_TYPE_DOUBLE:
				vu.ddouble = getDouble();
				break;

			case DVCFG_TYPE_STRING:
				if (readOnlyString) {
					vu.string = const_cast<char *>(getString().c_str());
				}
				else {
					vu.string = strdup(getString().c_str());
					sshsMemoryCheck(vu.string, "sshs_value.toCUnion");
				}
				break;

			case DVCFG_TYPE_UNKNOWN:
			default:
				throw std::runtime_error("SSHS: internal value type does not match any valid type.");
				break;
		}

		return (vu);
	}

	// Comparison operators.
	bool operator==(const sshs_value &rhs) const {
		switch (type) {
			case DVCFG_TYPE_BOOL:
				return (getBool() == rhs.getBool());

			case DVCFG_TYPE_INT:
				return (getInt() == rhs.getInt());

			case DVCFG_TYPE_LONG:
				return (getLong() == rhs.getLong());

			case DVCFG_TYPE_FLOAT:
				return (getFloat() == rhs.getFloat());

			case DVCFG_TYPE_DOUBLE:
				return (getDouble() == rhs.getDouble());

			case DVCFG_TYPE_STRING:
				return (getString() == rhs.getString());

			case DVCFG_TYPE_UNKNOWN:
			default:
				return (false);
		}
	}

	bool operator!=(const sshs_value &rhs) const {
		return (!this->operator==(rhs));
	}
};

// C++ helper functions.
const std::string &sshsHelperCppTypeToStringConverter(enum dvConfigAttributeType type);
enum dvConfigAttributeType sshsHelperCppStringToTypeConverter(const std::string &typeString);
std::string sshsHelperCppValueToStringConverter(const sshs_value &val);
sshs_value sshsHelperCppStringToValueConverter(enum dvConfigAttributeType type, const std::string &valueString);

// We don't care about unlocking anything here, as we exit hard on error anyway.
static inline void sshsNodeError(const std::string &funcName, const std::string &key,
	enum dvConfigAttributeType type, const std::string &msg, bool fatal = true) {
	boost::format errorMsg = boost::format("%s(): attribute '%s' (type '%s'): %s.") % funcName % key
							 % sshsHelperCppTypeToStringConverter(type) % msg;

	(*dvConfigTreeErrorLogCallbackGet())(errorMsg.str().c_str(), fatal);
}

static inline void sshsNodeErrorNoAttribute(
	const std::string &funcName, const std::string &key, enum dvConfigAttributeType type) {
	sshsNodeError(funcName, key, type, "attribute doesn't exist, you must create it first");
}

#endif /* CONFIG_TREE_INTERNAL_HPP_ */
