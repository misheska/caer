#ifndef CSTRING_HPP
#define CSTRING_HPP

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace dv {

class cstringIterator {
public:
	// Iterator traits.
	using iterator_category = std::random_access_iterator_tag;
	using value_type        = char;
	using pointer           = char *;
	using reference         = char &;
	using size_type         = size_t;
	using difference_type   = ptrdiff_t;

private:
	pointer element_ptr;

public:
	// Constructors.
	cstringIterator() : element_ptr(nullptr) {
	}

	cstringIterator(pointer _element_ptr) : element_ptr(_element_ptr) {
	}

	// Data access operators.
	reference operator*() const noexcept {
		return (*element_ptr);
	}

	pointer operator->() const noexcept {
		return (element_ptr);
	}

	reference operator[](size_type index) const noexcept {
		return (element_ptr[index]);
	}

	// Comparison operators.
	bool operator==(const cstringIterator &rhs) const noexcept {
		return (element_ptr == rhs.element_ptr);
	}

	bool operator!=(const cstringIterator &rhs) const noexcept {
		return (element_ptr != rhs.element_ptr);
	}

	bool operator<(const cstringIterator &rhs) const noexcept {
		return (element_ptr < rhs.element_ptr);
	}

	bool operator>(const cstringIterator &rhs) const noexcept {
		return (element_ptr > rhs.element_ptr);
	}

	bool operator<=(const cstringIterator &rhs) const noexcept {
		return (element_ptr <= rhs.element_ptr);
	}

	bool operator>=(const cstringIterator &rhs) const noexcept {
		return (element_ptr >= rhs.element_ptr);
	}

	// Prefix increment.
	cstringIterator &operator++() noexcept {
		element_ptr++;
		return (*this);
	}

	// Postfix increment.
	cstringIterator operator++(int) noexcept {
		return (cstringIterator(element_ptr++));
	}

	// Prefix decrement.
	cstringIterator &operator--() noexcept {
		element_ptr--;
		return (*this);
	}

	// Postfix decrement.
	cstringIterator operator--(int) noexcept {
		return (cstringIterator(element_ptr--));
	}

	// Iter += N.
	cstringIterator &operator+=(size_type add) noexcept {
		element_ptr += add;
		return (*this);
	}

	// Iter + N.
	cstringIterator operator+(size_type add) const noexcept {
		return (cstringIterator(element_ptr + add));
	}

	// N + Iter. Must be friend as Iter is right-hand-side.
	friend cstringIterator operator+(size_type lhs, const cstringIterator &rhs) noexcept {
		return (cstringIterator(rhs.element_ptr + lhs));
	}

	// Iter -= N.
	cstringIterator &operator-=(size_type sub) noexcept {
		element_ptr -= sub;
		return (*this);
	}

	// Iter - N. (N - Iter doesn't make sense!)
	cstringIterator operator-(size_type sub) const noexcept {
		return (cstringIterator(element_ptr - sub));
	}

	// Iter - Iter. (Iter + Iter doesn't make sense!)
	difference_type operator-(const cstringIterator &rhs) const noexcept {
		return (element_ptr - rhs.element_ptr);
	}

	// Swap two iterators.
	void swap(cstringIterator &rhs) noexcept {
		std::swap(element_ptr, rhs.element_ptr);
	}
};

class cstring {
public:
	// Container traits.
	using value_type       = char;
	using const_value_type = const char;
	using pointer          = char *;
	using const_pointer    = const char *;
	using reference        = char &;
	using const_reference  = const char &;
	using size_type        = size_t;
	using signed_size_type = ssize_t;
	using difference_type  = ptrdiff_t;

private:
	size_type curr_size;
	size_type max_size;
	pointer data_ptr;

public:
	cstring() {
	}

	cstring(const std::string &str) {
	}

	cstring &operator=(const std::string &str) {
	}

	size_t length() const {
	}
};

} // namespace dv

static_assert(std::is_standard_layout_v<dv::cstring>, "cstring is not standard layout");

#endif // CSTRING_HPP
