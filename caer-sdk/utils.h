#ifndef CAER_SDK_UTILS_H_
#define CAER_SDK_UTILS_H_

// Common includes, useful for everyone.
#include <libcaer/libcaer.h>

#include <libcaer/events/packetContainer.h>

#include "sshs/sshs.h"

// Suppress unused argument warnings, if needed
#define UNUSED_ARGUMENT(arg) (void) (arg)

#ifdef __cplusplus

#	include <libcaercpp/libcaer.hpp>

#	include <libcaercpp/events/packetContainer.hpp>

#	include "sshs/sshs.hpp"

#	include <algorithm>
#	include <vector>

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

template<class T> static void vectorSortUnique(std::vector<T> &vec) {
	std::sort(vec.begin(), vec.end());
	vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

template<class T> static bool vectorDetectDuplicates(std::vector<T> &vec) {
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

/**
 * Type that maps boolean expression to type.
 */
template<bool C> using enable_if_t = typename std::enable_if<C>::type;

/**
 * Type that maps a valid list of types to a type,
 * an invalid list of types to a type error.
 * @tparam A list of types that all have to be valid types
 */
template<typename...> struct void_t_impl { typedef void type; };

template<typename... T> using void_t = typename void_t_impl<T...>::type;

#endif

#endif /* CAER_SDK_UTILS_H_ */
