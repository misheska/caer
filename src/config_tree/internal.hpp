#ifndef CONFIG_TREE_INTERNAL_HPP_
#define CONFIG_TREE_INTERNAL_HPP_

// Implementation relevant common includes.
#include "caer-sdk/config/dvConfig.h"

#include <boost/format.hpp>
#include <cstring>
#include <stdexcept>
#include <string>
#include <variant>

// C linkage to guarantee no name mangling.
extern "C" {
// Internal node functions.
dvConfigNode dvConfigNodeNew(const char *nodeName, dvConfigNode parent, dvConfigTree global);
/**
 * This returns a reference to a node, and as such must be carefully mediated with
 * any dvConfigNodeRemoveNode() calls.
 */
dvConfigNode dvConfigNodeAddChild(dvConfigNode node, const char *childName);
/**
 * This returns a reference to a node, and as such must be carefully mediated with
 * any dvConfigNodeRemoveNode() calls.
 */
dvConfigNode dvConfigNodeGetChild(dvConfigNode node, const char *childName);
/**
 * Get link to global SSHS tree.
 */
dvConfigTree dvConfigNodeGetGlobal(dvConfigNode node);

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

class dv_ranges {
public:
	std::variant<int32_t, int64_t, float, double> min;
	std::variant<int32_t, int64_t, float, double> max;
	enum dv

		void
		fromCStruct(struct dvConfigAttributeRanges vs, enum dvConfigAttributeType ts) {
		switch (ts) {
			case DVCFG_TYPE_BOOL:
				// Nothing to set for bool.
				break;

			case DVCFG_TYPE_INT:
				min.emplace<int32_t>(vs.min.iintRange);
				max.emplace<int32_t>(vs.max.iintRange);
				break;

			case DVCFG_TYPE_LONG:
				min.emplace<int64_t>(vs.min.ilongRange);
				max.emplace<int64_t>(vs.max.ilongRange);
				break;

			case DVCFG_TYPE_FLOAT:
				min.emplace<float>(vs.min.ffloatRange);
				max.emplace<float>(vs.max.ffloatRange);
				break;

			case DVCFG_TYPE_DOUBLE:
				min.emplace<double>(vs.min.ddoubleRange);
				max.emplace<double>(vs.max.ddoubleRange);
				break;

			case DVCFG_TYPE_STRING:
				// String size is restricted to int32 internally.
				min.emplace<int32_t>(vs.min.stringRange);
				max.emplace<int32_t>(vs.max.stringRange);
				break;

			case DVCFG_TYPE_UNKNOWN:
			default:
				throw std::runtime_error("SSHS: provided union value type does not match any valid type.");
				break;
		}
	}

	struct dvConfigAttributeRanges toCStruct() const {
		struct dvConfigAttributeRanges vs;

		switch (getType()) {
			case DVCFG_TYPE_BOOL:
				// Set biggest value to 0 for booleans.
				vs.min.ilongRange = 0;
				vs.max.ilongRange = 0;
				break;

			case DVCFG_TYPE_INT:
				vs.min.iintRange = std::get<int32_t>(min);
				vs.max.iintRange = std::get<int32_t>(max);
				break;

			case DVCFG_TYPE_LONG:
				vs.min.ilongRange = std::get<int64_t>(min);
				vs.max.ilongRange = std::get<int64_t>(max);
				break;

			case DVCFG_TYPE_FLOAT:
				vs.min.ilongRange = 0;
				vs.max.ilongRange = 0;
				break;

			case DVCFG_TYPE_DOUBLE:
				vs.min.ilongRange = 0;
				vs.max.ilongRange = 0;
				break;

			case DVCFG_TYPE_STRING:
				vs.min.ilongRange = 0;
				vs.max.ilongRange = 0;
				break;

			case DVCFG_TYPE_UNKNOWN:
			default:
				throw std::runtime_error("SSHS: internal value type does not match any valid type.");
				break;
		}

		return (vs);
	}

	// Comparison operators.
	bool operator==(const dv_ranges &rhs) const {
		return (value == rhs.value);
	}

	bool operator!=(const dv_ranges &rhs) const {
		return (!this->operator==(rhs));
	}
};

class dv_value {
public:
	std::variant<bool, int32_t, int64_t, float, double, std::string> value;

	enum dvConfigAttributeType getType() const noexcept {
		return (static_cast<dvConfigAttributeType>(value.index()));
	}

	bool inRange(const struct dvConfigAttributeRanges &ranges) const noexcept {
		switch (getType()) {
			case DVCFG_TYPE_BOOL:
				// No check for bool, because no range exists.
				return (true);

			case DVCFG_TYPE_INT:
				return (getInt() >= ranges.min.iintRange && value.iint <= ranges.max.iintRange);

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
				value.emplace<bool>(vu.boolean);
				break;

			case DVCFG_TYPE_INT:
				value.emplace<int32_t>(vu.iint);
				break;

			case DVCFG_TYPE_LONG:
				value.emplace<int64_t>(vu.ilong);
				break;

			case DVCFG_TYPE_FLOAT:
				value.emplace<float>(vu.ffloat);
				break;

			case DVCFG_TYPE_DOUBLE:
				value.emplace<double>(vu.ddouble);
				break;

			case DVCFG_TYPE_STRING:
				value.emplace<std::string>(vu.string);
				break;

			case DVCFG_TYPE_UNKNOWN:
			default:
				throw std::runtime_error("SSHS: provided union value type does not match any valid type.");
				break;
		}
	}

	union dvConfigAttributeValue toCUnion(bool readOnlyString = false) const {
		union dvConfigAttributeValue vu;

		switch (getType()) {
			case DVCFG_TYPE_BOOL:
				vu.boolean = std::get<bool>(value);
				break;

			case DVCFG_TYPE_INT:
				vu.iint = std::get<int32_t>(value);
				break;

			case DVCFG_TYPE_LONG:
				vu.ilong = std::get<int64_t>(value);
				break;

			case DVCFG_TYPE_FLOAT:
				vu.ffloat = std::get<float>(value);
				break;

			case DVCFG_TYPE_DOUBLE:
				vu.ddouble = std::get<double>(value);
				break;

			case DVCFG_TYPE_STRING: {
				const std::string &str = std::get<std::string>(value);
				if (readOnlyString) {
					vu.string = const_cast<char *>(str.c_str());
				}
				else {
					vu.string = strdup(str.c_str());
					sshsMemoryCheck(vu.string, "sshs_value.toCUnion");
				}
				break;
			}

			case DVCFG_TYPE_UNKNOWN:
			default:
				throw std::runtime_error("SSHS: internal value type does not match any valid type.");
				break;
		}

		return (vu);
	}

	// Comparison operators.
	bool operator==(const dv_value &rhs) const {
		return (value == rhs.value);
	}

	bool operator!=(const dv_value &rhs) const {
		return (!this->operator==(rhs));
	}
};

// C++ helper functions.
const std::string &dvConfigHelperCppTypeToStringConverter(enum dvConfigAttributeType type);
enum dvConfigAttributeType dvConfigHelperCppStringToTypeConverter(const std::string &typeString);
std::string dvConfigHelperCppValueToStringConverter(const sshs_value &val);
sshs_value dvConfigHelperCppStringToValueConverter(enum dvConfigAttributeType type, const std::string &valueString);

// We don't care about unlocking anything here, as we exit hard on error anyway.
static inline void dvConfigNodeError(const std::string &funcName, const std::string &key,
	enum dvConfigAttributeType type, const std::string &msg, bool fatal = true) {
	boost::format errorMsg = boost::format("%s(): attribute '%s' (type '%s'): %s.") % funcName % key
							 % dvConfigHelperCppTypeToStringConverter(type) % msg;

	(*dvConfigTreeErrorLogCallbackGet())(errorMsg.str().c_str(), fatal);
}

static inline void dvConfigNodeErrorNoAttribute(
	const std::string &funcName, const std::string &key, enum dvConfigAttributeType type) {
	dvConfigNodeError(funcName, key, type, "attribute doesn't exist, you must create it first");
}

#endif /* CONFIG_TREE_INTERNAL_HPP_ */
