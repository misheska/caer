#include "internal.hpp"

#include <boost/tokenizer.hpp>

static const std::string typeStrings[] = {"bool", "byte", "short", "int", "long", "float", "double", "string"};

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
	else if (typeString == typeStrings[1]) {
		return (DVCFG_TYPE_INT);
	}
	else if (typeString == typeStrings[2]) {
		return (DVCFG_TYPE_INT);
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

std::string dvConfigHelperCppValueToStringConverter(const sshs_value &val) {
	// Convert the value and its type into a string for XML output.
	switch (val.getType()) {
		case DVCFG_TYPE_BOOL:
			// Manually generate true or false.
			return ((val.getBool()) ? ("true") : ("false"));

		case DVCFG_TYPE_INT:
			return (std::to_string(val.getInt()));

		case DVCFG_TYPE_LONG:
			return (std::to_string(val.getLong()));

		case DVCFG_TYPE_FLOAT:
			return (std::to_string(val.getFloat()));

		case DVCFG_TYPE_DOUBLE:
			return (std::to_string(val.getDouble()));

		case DVCFG_TYPE_STRING:
			return (val.getString());

		case DVCFG_TYPE_UNKNOWN:
		default:
			throw std::runtime_error("dvConfigHelperCppValueToStringConverter(): value has invalid type.");
	}
}

// Return false on failure (unknown type / faulty conversion), the content of
// value is undefined. For the STRING type, the returned value.string is a copy
// of the input string. Remember to free() it after use!
sshs_value dvConfigHelperCppStringToValueConverter(enum dvConfigAttributeType type, const std::string &valueString) {
	sshs_value value;

	switch (type) {
		case DVCFG_TYPE_BOOL:
			// Boolean uses custom true/false strings.
			value.setBool(valueString == "true");
			break;

		case DVCFG_TYPE_INT:
			value.setInt((int32_t) std::stol(valueString));
			break;

		case DVCFG_TYPE_LONG:
			value.setLong((int64_t) std::stoll(valueString));
			break;

		case DVCFG_TYPE_FLOAT:
			value.setFloat(std::stof(valueString));
			break;

		case DVCFG_TYPE_DOUBLE:
			value.setDouble(std::stod(valueString));
			break;

		case DVCFG_TYPE_STRING:
			value.setString(valueString);
			break;

		case DVCFG_TYPE_UNKNOWN:
		default:
			throw std::runtime_error("dvConfigHelperCppStringToValueConverter(): invalid type given.");
			break;
	}

	return (value);
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
	sshs_value val;
	val.fromCUnion(value, type);

	const std::string typeString = dvConfigHelperCppValueToStringConverter(val);

	char *resultString = strdup(typeString.c_str());
	sshsMemoryCheck(resultString, __func__);

	return (resultString);
}

// Remember to free the resulting union's "string" member, if the type was SSHS_STRING!
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
	else if (flags & DVCFG_FLAGS_NOTIFY_ONLY) {
		flagsStr = "NOTIFY_ONLY";
	}
	else {
		flagsStr = "NORMAL";
	}

	if (flags & DVCFG_FLAGS_NO_EXPORT) {
		flagsStr += "|NO_EXPORT";
	}

	char *resultString = strdup(flagsStr.c_str());
	sshsMemoryCheck(resultString, __func__);

	return (resultString);
}

int dvConfigHelperStringToFlagsConverter(const char *flagsString) {
	int flags = DVCFG_FLAGS_NORMAL;

	std::string flagsStr(flagsString);
	boost::tokenizer<boost::char_separator<char>> flagsTokens(flagsStr, boost::char_separator<char>("|"));

	// Search (or create) viable node iteratively.
	for (const auto &tok : flagsTokens) {
		if (tok == "READ_ONLY") {
			flags = DVCFG_FLAGS_READ_ONLY;
		}
		else if (tok == "NOTIFY_ONLY") {
			flags = DVCFG_FLAGS_NOTIFY_ONLY;
		}
		else if (tok == "NO_EXPORT") {
			flags |= DVCFG_FLAGS_NO_EXPORT;
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
			snprintf(buf, 256, "%" PRIi32 "|%" PRIi32, ranges.min.iintRange, ranges.max.iintRange);
			break;

		case DVCFG_TYPE_LONG:
			snprintf(buf, 256, "%" PRIi64 "|%" PRIi64, ranges.min.ilongRange, ranges.max.ilongRange);
			break;

		case DVCFG_TYPE_FLOAT:
			snprintf(buf, 256, "%g|%g", (double) ranges.min.ffloatRange, (double) ranges.max.ffloatRange);
			break;

		case DVCFG_TYPE_DOUBLE:
			snprintf(buf, 256, "%g|%g", ranges.min.ddoubleRange, ranges.max.ddoubleRange);
			break;

		case DVCFG_TYPE_STRING:
			snprintf(buf, 256, "%zu|%zu", ranges.min.stringRange, ranges.max.stringRange);
			break;
	}

	char *resultString = strdup(buf);
	sshsMemoryCheck(resultString, __func__);

	return (resultString);
}

struct dvConfigAttributeRanges dvConfigHelperStringToRangesConverter(
	enum dvConfigAttributeType type, const char *rangesString) {
	struct dvConfigAttributeRanges ranges;

	switch (type) {
		case DVCFG_TYPE_UNKNOWN:
		case DVCFG_TYPE_BOOL:
			ranges.min.ilongRange = 0;
			ranges.max.ilongRange = 0;
			break;

		case DVCFG_TYPE_INT:
			sscanf(rangesString, "%" SCNi32 "|%" SCNi32, &ranges.min.iintRange, &ranges.max.iintRange);
			break;

		case DVCFG_TYPE_LONG:
			sscanf(rangesString, "%" SCNi64 "|%" SCNi64, &ranges.min.ilongRange, &ranges.max.ilongRange);
			break;

		case DVCFG_TYPE_FLOAT:
			sscanf(rangesString, "%g|%g", &ranges.min.ffloatRange, &ranges.max.ffloatRange);
			break;

		case DVCFG_TYPE_DOUBLE:
			sscanf(rangesString, "%lg|%lg", &ranges.min.ddoubleRange, &ranges.max.ddoubleRange);
			break;

		case DVCFG_TYPE_STRING:
			sscanf(rangesString, "%zu|%zu", &ranges.min.stringRange, &ranges.max.stringRange);
			break;
	}

	return (ranges);
}
