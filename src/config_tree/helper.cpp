#include "internal.hpp"

#include <boost/tokenizer.hpp>

static const std::string typeStrings[] = {"bool", "int", "long", "float", "double", "string"};

const std::string &dvConfigHelperCppTypeToStringConverter(enum dvConfigAttributeType type) {
	// Convert the value and its type into a string for XML output.
	switch (type) {
		case DVCFG_TYPE_BOOL:
		case DVCFG_TYPE_INT:
		case DVCFG_TYPE_LONG:
		case DVCFG_TYPE_FLOAT:
		case DVCFG_TYPE_DOUBLE:
		case DVCFG_TYPE_STRING:
			return (typeStrings[type]);

		case DVCFG_TYPE_UNKNOWN:
		default:
			throw std::runtime_error("dvConfigHelperCppTypeToStringConverter(): invalid type given.");
	}
}

enum dvConfigAttributeType dvConfigHelperCppStringToTypeConverter(const std::string &typeString) {
	// Convert the value string back into the internal type representation.
	if (typeString == typeStrings[DVCFG_TYPE_BOOL]) {
		return (DVCFG_TYPE_BOOL);
	}
	else if (typeString == typeStrings[DVCFG_TYPE_INT]) {
		return (DVCFG_TYPE_INT);
	}
	else if (typeString == typeStrings[DVCFG_TYPE_LONG]) {
		return (DVCFG_TYPE_LONG);
	}
	else if (typeString == typeStrings[DVCFG_TYPE_FLOAT]) {
		return (DVCFG_TYPE_FLOAT);
	}
	else if (typeString == typeStrings[DVCFG_TYPE_DOUBLE]) {
		return (DVCFG_TYPE_DOUBLE);
	}
	else if (typeString == typeStrings[DVCFG_TYPE_STRING]) {
		return (DVCFG_TYPE_STRING);
	}

	return (DVCFG_TYPE_UNKNOWN); // UNKNOWN TYPE.
}

const std::string dvConfigHelperCppValueToStringConverter(const dv_value &val) {
	// Convert the value and its type into a string for XML output.
	switch (val.getType()) {
		case DVCFG_TYPE_BOOL:
			// Manually generate true or false.
			return ((std::get<bool>(val)) ? ("true") : ("false"));

		case DVCFG_TYPE_INT:
			return (std::to_string(std::get<int32_t>(val)));

		case DVCFG_TYPE_LONG:
			return (std::to_string(std::get<int64_t>(val)));

		case DVCFG_TYPE_FLOAT:
			return (std::to_string(std::get<float>(val)));

		case DVCFG_TYPE_DOUBLE:
			return (std::to_string(std::get<double>(val)));

		case DVCFG_TYPE_STRING:
			return (std::get<std::string>(val));

		case DVCFG_TYPE_UNKNOWN:
		default:
			throw std::runtime_error("dvConfigHelperCppValueToStringConverter(): value has invalid type.");
	}
}

// Return false on failure (unknown type / faulty conversion), the content of
// value is undefined. For the STRING type, the returned value.string is a copy
// of the input string. Remember to free() it after use!
dv_value dvConfigHelperCppStringToValueConverter(enum dvConfigAttributeType type, const std::string &valueString) {
	dv_value val;

	switch (type) {
		case DVCFG_TYPE_BOOL:
			// Boolean uses custom true/false strings.
			val.emplace<bool>(valueString == "true");
			break;

		case DVCFG_TYPE_INT:
			val.emplace<int32_t>(std::stol(valueString));
			break;

		case DVCFG_TYPE_LONG:
			val.emplace<int64_t>(std::stoll(valueString));
			break;

		case DVCFG_TYPE_FLOAT:
			val.emplace<float>(std::stof(valueString));
			break;

		case DVCFG_TYPE_DOUBLE:
			val.emplace<double>(std::stod(valueString));
			break;

		case DVCFG_TYPE_STRING:
			val.emplace<std::string>(valueString);
			break;

		case DVCFG_TYPE_UNKNOWN:
		default:
			throw std::runtime_error("dvConfigHelperCppStringToValueConverter(): invalid type given.");
			break;
	}

	return (val);
}

/**
 * C11 wrappers for external use.
 */

// Do not free or modify the resulting string in any way!
const char *dvConfigHelperTypeToStringConverter(enum dvConfigAttributeType type) {
	return (dvConfigHelperCppTypeToStringConverter(type).c_str());
}

enum dvConfigAttributeType dvConfigHelperStringToTypeConverter(const char *typeString) {
	return (dvConfigHelperCppStringToTypeConverter(typeString));
}

// Remember to free the resulting string!
char *dvConfigHelperValueToStringConverter(enum dvConfigAttributeType type, union dvConfigAttributeValue value) {
	dv_value val;
	val.fromCUnion(value, type);

	const std::string typeString = dvConfigHelperCppValueToStringConverter(val);

	char *resultString = strdup(typeString.c_str());
	dvConfigMemoryCheck(resultString, __func__);

	return (resultString);
}

// Remember to free the resulting union's "string" member, if the type was DVCFG_TYPE_STRING!
union dvConfigAttributeValue dvConfigHelperStringToValueConverter(
	enum dvConfigAttributeType type, const char *valueString) {
	if ((type == DVCFG_TYPE_STRING) && (valueString == nullptr)) {
		// Empty string.
		valueString = "";
	}

	return (dvConfigHelperCppStringToValueConverter(type, valueString).toCUnion());
}

char *dvConfigHelperFlagsToStringConverter(int flags) {
	std::string flagsStr;

	if (flags & DVCFG_FLAGS_READ_ONLY) {
		flagsStr = "READ_ONLY";
	}
	else {
		flagsStr = "NORMAL";
	}

	if (flags & DVCFG_FLAGS_NO_EXPORT) {
		flagsStr += "|NO_EXPORT";
	}

	if (flags & DVCFG_FLAGS_IMPORTED) {
		flagsStr += "|IMPORTED";
	}

	char *resultString = strdup(flagsStr.c_str());
	dvConfigMemoryCheck(resultString, __func__);

	return (resultString);
}

int dvConfigHelperStringToFlagsConverter(const char *flagsString) {
	int flags = DVCFG_FLAGS_NORMAL;

	std::string flagsStr(flagsString);
	boost::tokenizer<boost::char_separator<char>> flagsTokens(flagsStr, boost::char_separator<char>("|", nullptr));

	// Search (or create) viable node iteratively.
	for (const auto &tok : flagsTokens) {
		if (tok == "READ_ONLY") {
			flags |= DVCFG_FLAGS_READ_ONLY;
		}
		else if (tok == "NO_EXPORT") {
			flags |= DVCFG_FLAGS_NO_EXPORT;
		}
		else if (tok == "IMPORTED") {
			flags |= DVCFG_FLAGS_IMPORTED;
		}
	}

	return (flags);
}

char *dvConfigHelperRangesToStringConverter(enum dvConfigAttributeType type, struct dvConfigAttributeRanges ranges) {
	// We need to return a string with the two ranges,
	// separated by a | character.
	char buf[256];

	switch (type) {
		case DVCFG_TYPE_UNKNOWN:
		case DVCFG_TYPE_BOOL:
			snprintf(buf, 256, "0|0");
			break;

		case DVCFG_TYPE_INT:
			snprintf(buf, 256, "%" PRIi32 "|%" PRIi32, ranges.min.intRange, ranges.max.intRange);
			break;

		case DVCFG_TYPE_LONG:
			snprintf(buf, 256, "%" PRIi64 "|%" PRIi64, ranges.min.longRange, ranges.max.longRange);
			break;

		case DVCFG_TYPE_FLOAT:
			snprintf(buf, 256, "%g|%g", static_cast<double>(ranges.min.floatRange),
				static_cast<double>(ranges.max.floatRange));
			break;

		case DVCFG_TYPE_DOUBLE:
			snprintf(buf, 256, "%g|%g", ranges.min.doubleRange, ranges.max.doubleRange);
			break;

		case DVCFG_TYPE_STRING:
			snprintf(buf, 256, "%" PRIi32 "|%" PRIi32, ranges.min.stringRange, ranges.max.stringRange);
			break;
	}

	char *resultString = strdup(buf);
	dvConfigMemoryCheck(resultString, __func__);

	return (resultString);
}

struct dvConfigAttributeRanges dvConfigHelperStringToRangesConverter(
	enum dvConfigAttributeType type, const char *rangesString) {
	struct dvConfigAttributeRanges ranges;

	switch (type) {
		case DVCFG_TYPE_UNKNOWN:
		case DVCFG_TYPE_BOOL:
			ranges.min.intRange = 0;
			ranges.max.intRange = 0;
			break;

		case DVCFG_TYPE_INT:
			sscanf(rangesString, "%" SCNi32 "|%" SCNi32, &ranges.min.intRange, &ranges.max.intRange);
			break;

		case DVCFG_TYPE_LONG:
			sscanf(rangesString, "%" SCNi64 "|%" SCNi64, &ranges.min.longRange, &ranges.max.longRange);
			break;

		case DVCFG_TYPE_FLOAT:
			sscanf(rangesString, "%g|%g", &ranges.min.floatRange, &ranges.max.floatRange);
			break;

		case DVCFG_TYPE_DOUBLE:
			sscanf(rangesString, "%lg|%lg", &ranges.min.doubleRange, &ranges.max.doubleRange);
			break;

		case DVCFG_TYPE_STRING:
			sscanf(rangesString, "%" SCNi32 "|%" SCNi32, &ranges.min.stringRange, &ranges.max.stringRange);
			break;
	}

	return (ranges);
}
