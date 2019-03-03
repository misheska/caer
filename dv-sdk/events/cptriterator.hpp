#ifndef CPTRITERATOR_HPP
#define CPTRITERATOR_HPP

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <iterator>

namespace dv {

template<class T> class cPtrIterator {
public:
	// Iterator traits.
	using iterator_category = std::random_access_iterator_tag;
	using value_type        = typename std::remove_cv_t<T>;
	using pointer           = T *;
	using reference         = T &;
	using size_type         = size_t;
	using difference_type   = ptrdiff_t;

private:
	pointer element_ptr;

public:
	// Constructors.
	cPtrIterator() : element_ptr(nullptr) {
	}

	cPtrIterator(pointer _element_ptr) : element_ptr(_element_ptr) {
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
	bool operator==(const cPtrIterator &rhs) const noexcept {
		return (element_ptr == rhs.element_ptr);
	}

	bool operator!=(const cPtrIterator &rhs) const noexcept {
		return (element_ptr != rhs.element_ptr);
	}

	bool operator<(const cPtrIterator &rhs) const noexcept {
		return (element_ptr < rhs.element_ptr);
	}

	bool operator>(const cPtrIterator &rhs) const noexcept {
		return (element_ptr > rhs.element_ptr);
	}

	bool operator<=(const cPtrIterator &rhs) const noexcept {
		return (element_ptr <= rhs.element_ptr);
	}

	bool operator>=(const cPtrIterator &rhs) const noexcept {
		return (element_ptr >= rhs.element_ptr);
	}

	// Prefix increment.
	cPtrIterator &operator++() noexcept {
		element_ptr++;
		return (*this);
	}

	// Postfix increment.
	cPtrIterator operator++(int) noexcept {
		return (cPtrIterator(element_ptr++));
	}

	// Prefix decrement.
	cPtrIterator &operator--() noexcept {
		element_ptr--;
		return (*this);
	}

	// Postfix decrement.
	cPtrIterator operator--(int) noexcept {
		return (cPtrIterator(element_ptr--));
	}

	// Iter += N.
	cPtrIterator &operator+=(size_type add) noexcept {
		element_ptr += add;
		return (*this);
	}

	// Iter + N.
	cPtrIterator operator+(size_type add) const noexcept {
		return (cPtrIterator(element_ptr + add));
	}

	// N + Iter. Must be friend as Iter is right-hand-side.
	friend cPtrIterator operator+(size_type lhs, const cPtrIterator &rhs) noexcept {
		return (cPtrIterator(rhs.element_ptr + lhs));
	}

	// Iter -= N.
	cPtrIterator &operator-=(size_type sub) noexcept {
		element_ptr -= sub;
		return (*this);
	}

	// Iter - N. (N - Iter doesn't make sense!)
	cPtrIterator operator-(size_type sub) const noexcept {
		return (cPtrIterator(element_ptr - sub));
	}

	// Iter - Iter. (Iter + Iter doesn't make sense!)
	difference_type operator-(const cPtrIterator &rhs) const noexcept {
		return (element_ptr - rhs.element_ptr);
	}

	// Swap two iterators.
	void swap(cPtrIterator &rhs) noexcept {
		std::swap(element_ptr, rhs.element_ptr);
	}
};

} // namespace dv

#endif // CPTRITERATOR_HPP
