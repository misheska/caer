#ifndef CONFIG_TREE_INTERNAL_HPP_
#define CONFIG_TREE_INTERNAL_HPP_

// Implementation relevant common includes.
#include "dv-sdk/config/dvConfig.h"

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
 * Get link to global configuration tree.
 */
dvConfigTree dvConfigNodeGetGlobal(dvConfigNode node);

// Internal global functions.
dvConfigNodeChangeListener dvConfigGlobalNodeListenerGetFunction(dvConfigTree tree);
void *dvConfigGlobalNodeListenerGetUserData(dvConfigTree tree);
dvConfigAttributeChangeListener dvConfigGlobalAttributeListenerGetFunction(dvConfigTree tree);
void *dvConfigGlobalAttributeListenerGetUserData(dvConfigTree tree);
}

template<typename InIter, typename Elem> static inline bool findBool(InIter begin, InIter end, const Elem &val) {
	const auto result = std::find(begin, end, val);

	if (result == end) {
		return (false);
	}

	return (true);
}

// Terminate process on failed memory allocation.
template<typename T> static inline void dvConfigMemoryCheck(T *ptr, const std::string &funcName) {
	if (ptr == nullptr) {
		boost::format errorMsg = boost::format("%s(): unable to allocate memory.") % funcName;

		(*dvConfigTreeErrorLogCallbackGet())(errorMsg.str().c_str(), true);
	}
}

class dv_ranges {
public:
	using RangeVariant = std::variant<int32_t, int64_t, float, double>;

	RangeVariant min;
	RangeVariant max;

	void fromCStruct(struct dvConfigAttributeRanges vs, enum dvConfigAttributeType ts) {
		switch (ts) {
			case DVCFG_TYPE_BOOL:
				// Nothing to set for bool.
				min.emplace<int32_t>(0);
				max.emplace<int32_t>(0);
				break;

			case DVCFG_TYPE_INT:
				min.emplace<int32_t>(vs.min.intRange);
				max.emplace<int32_t>(vs.max.intRange);
				break;

			case DVCFG_TYPE_LONG:
				min.emplace<int64_t>(vs.min.longRange);
				max.emplace<int64_t>(vs.max.longRange);
				break;

			case DVCFG_TYPE_FLOAT:
				min.emplace<float>(vs.min.floatRange);
				max.emplace<float>(vs.max.floatRange);
				break;

			case DVCFG_TYPE_DOUBLE:
				min.emplace<double>(vs.min.doubleRange);
				max.emplace<double>(vs.max.doubleRange);
				break;

			case DVCFG_TYPE_STRING:
				// String size is restricted to int32 internally.
				min.emplace<int32_t>(vs.min.stringRange);
				max.emplace<int32_t>(vs.max.stringRange);
				break;

			case DVCFG_TYPE_UNKNOWN:
			default:
				throw std::runtime_error("dvConfig: provided union value type does not match any valid type.");
				break;
		}
	}

	struct dvConfigAttributeRanges toCStruct() const {
		struct dvConfigAttributeRanges vs;

		switch (min.index()) {
			case 0: // int32
				vs.min.intRange = std::get<0>(min);
				vs.max.intRange = std::get<0>(max);
				break;

			case 1: // int64
				vs.min.longRange = std::get<1>(min);
				vs.max.longRange = std::get<1>(max);
				break;

			case 2: // float
				vs.min.floatRange = std::get<2>(min);
				vs.max.floatRange = std::get<2>(max);
				break;

			case 3: // double
				vs.min.doubleRange = std::get<3>(min);
				vs.max.doubleRange = std::get<3>(max);
				break;
		}

		return (vs);
	}

	// Comparison operators.
	bool operator==(const dv_ranges &rhs) const {
		return ((min == rhs.min) && (max == rhs.max));
	}

	bool operator!=(const dv_ranges &rhs) const {
		return (!this->operator==(rhs));
	}
};

class dv_value : public std::variant<bool, int32_t, int64_t, float, double, std::string> {
public:
	enum dvConfigAttributeType getType() const noexcept {
		return (static_cast<dvConfigAttributeType>(index()));
	}

	bool inRange(const dv_ranges &ranges) const noexcept {
		switch (getType()) {
			case DVCFG_TYPE_BOOL:
				// No check for bool, because no range exists.
				return (true);

			case DVCFG_TYPE_INT:
				return ((std::get<int32_t>(*this) >= std::get<int32_t>(ranges.min))
						&& (std::get<int32_t>(*this) <= std::get<int32_t>(ranges.max)));

			case DVCFG_TYPE_LONG:
				return ((std::get<int64_t>(*this) >= std::get<int64_t>(ranges.min))
						&& (std::get<int64_t>(*this) <= std::get<int64_t>(ranges.max)));

			case DVCFG_TYPE_FLOAT:
				return ((std::get<float>(*this) >= std::get<float>(ranges.min))
						&& (std::get<float>(*this) <= std::get<float>(ranges.max)));

			case DVCFG_TYPE_DOUBLE:
				return ((std::get<double>(*this) >= std::get<double>(ranges.min))
						&& (std::get<double>(*this) <= std::get<double>(ranges.max)));

			case DVCFG_TYPE_STRING:
				return (
					(std::get<std::string>(*this).length() >= static_cast<size_t>(std::get<int32_t>(ranges.min)))
					&& (std::get<std::string>(*this).length() <= static_cast<size_t>(std::get<int32_t>(ranges.max))));

			case DVCFG_TYPE_UNKNOWN:
			default:
				return (false);
		}
	}

	void fromCUnion(union dvConfigAttributeValue vu, enum dvConfigAttributeType tu) {
		switch (tu) {
			case DVCFG_TYPE_BOOL:
				emplace<bool>(vu.boolean);
				break;

			case DVCFG_TYPE_INT:
				emplace<int32_t>(vu.iint);
				break;

			case DVCFG_TYPE_LONG:
				emplace<int64_t>(vu.ilong);
				break;

			case DVCFG_TYPE_FLOAT:
				emplace<float>(vu.ffloat);
				break;

			case DVCFG_TYPE_DOUBLE:
				emplace<double>(vu.ddouble);
				break;

			case DVCFG_TYPE_STRING:
				emplace<std::string>(vu.string);
				break;

			case DVCFG_TYPE_UNKNOWN:
			default:
				throw std::runtime_error("dvConfig: provided union value type does not match any valid type.");
				break;
		}
	}

	union dvConfigAttributeValue toCUnion(bool readOnlyString = false) const {
		union dvConfigAttributeValue vu;

		switch (getType()) {
			case DVCFG_TYPE_BOOL:
				vu.boolean = std::get<bool>(*this);
				break;

			case DVCFG_TYPE_INT:
				vu.iint = std::get<int32_t>(*this);
				break;

			case DVCFG_TYPE_LONG:
				vu.ilong = std::get<int64_t>(*this);
				break;

			case DVCFG_TYPE_FLOAT:
				vu.ffloat = std::get<float>(*this);
				break;

			case DVCFG_TYPE_DOUBLE:
				vu.ddouble = std::get<double>(*this);
				break;

			case DVCFG_TYPE_STRING: {
				const std::string &str = std::get<std::string>(*this);
				if (readOnlyString) {
					vu.string = const_cast<char *>(str.c_str());
				}
				else {
					vu.string = strdup(str.c_str());
					dvConfigMemoryCheck(vu.string, "dv_value.toCUnion");
				}
				break;
			}

			case DVCFG_TYPE_UNKNOWN:
			default:
				throw std::runtime_error("dvConfig: internal value type does not match any valid type.");
				break;
		}

		return (vu);
	}
};

// C++ helper functions.
const std::string &dvConfigHelperCppTypeToStringConverter(enum dvConfigAttributeType type);
enum dvConfigAttributeType dvConfigHelperCppStringToTypeConverter(const std::string &typeString);
const std::string dvConfigHelperCppValueToStringConverter(const dv_value &val);
dv_value dvConfigHelperCppStringToValueConverter(enum dvConfigAttributeType type, const std::string &valueString);

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
