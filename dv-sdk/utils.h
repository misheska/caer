#ifndef DV_SDK_UTILS_H_
#define DV_SDK_UTILS_H_

// Common includes, useful for everyone.
#include <libcaer/libcaer.h>

#include "config/dvConfig.h"

// Suppress unused argument warnings, if needed
#define UNUSED_ARGUMENT(arg) (void) (arg)

#ifdef __cplusplus
extern "C" {
#endif

void dvLog(enum caer_log_level level, const char *format, ...) ATTRIBUTE_FORMAT(2);

#ifdef __cplusplus
}

#	include <libcaercpp/libcaer.hpp>

#	include "config/dvConfig.hpp"

#	include <algorithm>
#	include <vector>
#	include <utility>
#	include <boost/format.hpp>

namespace dv {

using logLevel = libcaer::log::logLevel;

template<typename... Args> static inline void Log(logLevel level, const char *format, Args &&... args) {
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wformat-nonliteral"
	dvLog(static_cast<enum caer_log_level>(level), format, std::forward<Args>(args)...);
#	pragma GCC diagnostic pop
}

template<typename... Args> static inline void Log(logLevel level, const std::string &format, Args &&... args) {
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wformat-nonliteral"
	dvLog(static_cast<enum caer_log_level>(level), format.c_str(), std::forward<Args>(args)...);
#	pragma GCC diagnostic pop
}

static inline void Log(logLevel level, const boost::format &format) {
	dvLog(static_cast<enum caer_log_level>(level), "%s", format.str().c_str());
}

template<typename InIter, typename Elem> static inline bool findBool(InIter begin, InIter end, const Elem &val) {
	const auto result = std::find(begin, end, val);

	if (result == end) {
		return (false);
	}

	return (true);
}

template<typename InIter, typename Pred> static inline bool findIfBool(InIter begin, InIter end, Pred predicate) {
	const auto result = std::find_if(begin, end, predicate);

	if (result == end) {
		return (false);
	}

	return (true);
}

template<typename T> static inline void vectorSortUnique(std::vector<T> &vec) {
	std::sort(vec.begin(), vec.end());
	vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

template<typename T> static inline bool vectorDetectDuplicates(std::vector<T> &vec) {
	// Detect duplicates.
	size_t sizeBefore = vec.size();

	vectorSortUnique(vec);

	size_t sizeAfter = vec.size();

	// If size changed, duplicates must have been removed, so they existed
	// in the first place!
	if (sizeAfter != sizeBefore) {
		return (true);
	}

	return (false);
}

} // namespace dv

#endif

#endif /* DV_SDK_UTILS_H_ */
