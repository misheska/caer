#ifndef CSTRING_HPP
#define CSTRING_HPP

#include "cptriterator.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dv {

template<class T> class basic_cstring {
public:
	// Container traits.
	using value_type       = T;
	using const_value_type = const T;
	using pointer          = T *;
	using const_pointer    = const T *;
	using reference        = T &;
	using const_reference  = const T &;
	using size_type        = size_t;
	using difference_type  = ptrdiff_t;

	static const size_type npos = static_cast<size_type>(-1);

	static_assert(std::is_pod_v<value_type>, "basic_cstring type is not POD");

private:
	size_type curr_size;
	size_type maximum_size;
	pointer data_ptr;

public:
	// Default constructor. Initialize empty string with space for 64 characters.
	basic_cstring() {
		curr_size = 0;

		allocateMemory(64);
		nullTerminate();
	}

	// Destructor.
	~basic_cstring() noexcept {
		curr_size = 0;

		freeMemory();
	}

	// Copy constructor.
	basic_cstring(const basic_cstring &str, size_type pos = 0, size_type count = npos) :
		basic_cstring(str.c_str(), str.length(), pos, count) {
	}

	basic_cstring(std::basic_string_view<value_type> str, size_type pos = 0, size_type count = npos) :
		basic_cstring(str.data(), str.size(), pos, count) {
	}

	basic_cstring(const_pointer str) : basic_cstring(str, strlen(str)) {
	}

	// Lowest common denominator: a ptr and sizes. Most constructors call this.
	basic_cstring(const_pointer str, size_type strLength, size_type pos = 0, size_type count = npos) {
		if (str == nullptr) {
			throw std::invalid_argument("string resolves to nullptr.");
		}

		if (pos > strLength) {
			throw std::length_error("position bigger than string length.");
		}

		// Ensure number of characters to copy is within range.
		if (count > (strLength - pos)) {
			count = strLength - pos;
		}

		curr_size = count;

		allocateMemory(count);

		std::copy_n(const_iterator(str + pos), count, begin());
		nullTerminate();
	}

	// Initialize string with N times the given character.
	basic_cstring(size_type count, value_type value) {
		curr_size = count;

		allocateMemory(count);

		// Initialize elements to copy of value.
		std::fill_n(begin(), count, value);
		nullTerminate();
	}

	// Initialize string with characters from range.
	template<class InputIt> basic_cstring(InputIt first, InputIt last) {
		auto difference = std::distance(first, last);
		if (difference < 0) {
			throw std::invalid_argument("Inverted iterators (last < first). This is never what you really want.");
		}

		auto count = static_cast<size_type>(difference);

		curr_size = count;

		allocateMemory(count);

		// Initialize elements to copy of range's values.
		std::copy_n(first, count, begin());
		nullTerminate();
	}

	// Initialize vector via initializer list {x, y, z}.
	basic_cstring(std::initializer_list<value_type> init_list) : basic_cstring(init_list.begin(), init_list.end()) {
	}

	// Move constructor.
	basic_cstring(basic_cstring &&rhs) noexcept {
		// Moved-from object must remain in a valid state. We can define
		// valid-state-after-move to be nothing allowed but a destructor
		// call, which is what normally happens, and helps us a lot here.

		// Move data here.
		curr_size    = rhs.curr_size;
		maximum_size = rhs.maximum_size;
		data_ptr     = rhs.data_ptr;

		// Reset old data (ready for destruction).
		rhs.curr_size    = 0;
		rhs.maximum_size = 0;
		rhs.data_ptr     = nullptr;
	}

	// Move assignment.
	basic_cstring &operator=(basic_cstring &&rhs) noexcept {
		return (assign(std::move(rhs)));
	}

	// Copy assignment.
	basic_cstring &operator=(const basic_cstring &rhs) {
		return (assign(rhs));
	}

	// Extra assignment operators.
	basic_cstring &operator=(std::basic_string_view<value_type> rhs) {
		return (assign(rhs));
	}

	basic_cstring &operator=(const_pointer rhs) {
		return (assign(rhs));
	}

	basic_cstring &operator=(value_type value) {
		return (assign(1, value));
	}

	basic_cstring &operator=(std::initializer_list<value_type> rhs_list) {
		return (assign(rhs_list));
	}

	// Comparison operators.
	bool operator==(const basic_cstring &rhs) const noexcept {
		return (std::equal(cbegin(), cend(), rhs.cbegin(), rhs.cend()));
	}

	bool operator!=(const basic_cstring &rhs) const noexcept {
		return (!operator==(rhs));
	}

	bool operator==(std::basic_string_view<value_type> rhs) const noexcept {
		return (std::equal(cbegin(), cend(), rhs.cbegin(), rhs.cend()));
	}

	bool operator!=(std::basic_string_view<value_type> rhs) const noexcept {
		return (!operator==(rhs));
	}

	friend bool operator==(std::basic_string_view<value_type> lhs, const basic_cstring &rhs) noexcept {
		return (rhs.operator==(lhs));
	}

	friend bool operator!=(std::basic_string_view<value_type> lhs, const basic_cstring &rhs) noexcept {
		return (rhs.operator!=(lhs));
	}

	bool operator==(const_pointer rhs) const noexcept {
		return (std::equal(cbegin(), cend(), const_iterator(rhs), const_iterator(rhs + strlen(rhs))));
	}

	bool operator!=(const_pointer rhs) const noexcept {
		return (!operator==(rhs));
	}

	friend bool operator==(const_pointer lhs, const basic_cstring &rhs) noexcept {
		return (rhs.operator==(lhs));
	}

	friend bool operator!=(const_pointer lhs, const basic_cstring &rhs) noexcept {
		return (rhs.operator!=(lhs));
	}

	basic_cstring &assign(basic_cstring &&str) noexcept {
		assert(this != &str);

		// Moved-from object must remain in a valid state. We can define
		// valid-state-after-move to be nothing allowed but a destructor
		// call, which is what normally happens, and helps us a lot here.

		// Destroy current data.
		curr_size = 0;

		freeMemory();

		// Move data here.
		curr_size    = str.curr_size;
		maximum_size = str.maximum_size;
		data_ptr     = str.data_ptr;

		// Reset old data (ready for destruction).
		str.curr_size    = 0;
		str.maximum_size = 0;
		str.data_ptr     = nullptr;

		return (*this);
	}

	basic_cstring &assign(const basic_cstring &str, size_type pos = 0, size_type count = npos) {
		// If operation would have no effect, do nothing.
		if ((this == &str) && (pos == 0) && (count >= str.length())) {
			return (*this);
		}

		return (assign(str.c_str(), str.length(), pos, count));
	}

	basic_cstring &assign(std::basic_string_view<value_type> str, size_type pos = 0, size_type count = npos) {
		return (assign(str.data(), str.size(), pos, count));
	}

	basic_cstring &assign(const_pointer str) {
		return (assign(str, strlen(str)));
	}

	// Lowest common denominator: a ptr and sizes. Most assignments call this.
	basic_cstring &assign(const_pointer str, size_type strLength, size_type pos = 0, size_type count = npos) {
		if (str == nullptr) {
			throw std::invalid_argument("string resolves to nullptr.");
		}

		if (pos > strLength) {
			throw std::length_error("position bigger than string length.");
		}

		// Ensure number of characters to copy is within range.
		if (count > (strLength - pos)) {
			count = strLength - pos;
		}

		ensureCapacity(count);

		curr_size = count;

		std::copy_n(const_iterator(str + pos), count, begin());
		nullTerminate();

		return (*this);
	}

	// Replace string with N times the given character.
	basic_cstring &assign(size_type count, value_type value) {
		ensureCapacity(count);

		curr_size = count;

		// Initialize elements to copy of value.
		std::fill_n(begin(), count, value);
		nullTerminate();

		return (*this);
	}

	// Replace string with characters from range.
	template<class InputIt> basic_cstring &assign(InputIt first, InputIt last) {
		auto difference = std::distance(first, last);
		if (difference < 0) {
			throw std::invalid_argument("Inverted iterators (last < first). This is never what you really want.");
		}

		auto count = static_cast<size_type>(difference);

		ensureCapacity(count);

		curr_size = count;

		// Initialize elements to copy of range's values.
		std::copy_n(first, count, begin());
		nullTerminate();

		return (*this);
	}

	// Replace string via initializer list {x, y, z}.
	basic_cstring &assign(std::initializer_list<value_type> init_list) {
		return (assign(init_list.begin(), init_list.end()));
	}

	pointer data() noexcept {
		return (data_ptr);
	}

	const_pointer data() const noexcept {
		return (data_ptr);
	}

	const_pointer c_str() const noexcept {
		return (data());
	}

	size_type size() const noexcept {
		return (curr_size);
	}

	size_type length() const noexcept {
		return (size());
	}

	size_type capacity() const noexcept {
		return (maximum_size);
	}

	size_type max_size() const noexcept {
		// -1 to account for NULL termination.
		return (static_cast<size_type>(std::numeric_limits<difference_type>::max() - 1));
	}

	[[nodiscard]] bool empty() const noexcept {
		return (curr_size == 0);
	}

	void resize(size_type newSize, value_type value) {
		ensureCapacity(newSize);

		if (newSize >= curr_size) {
			// Add new characters on expansion.
			std::fill_n(end(), (newSize - curr_size), value);
		}

		curr_size = newSize;
		nullTerminate();
	}

	void reserve(size_type minCapacity) {
		ensureCapacity(minCapacity);
	}

	void shrink_to_fit() {
		if (curr_size == maximum_size) {
			return; // Already smallest possible size.
		}

		// Capacity is bigger than size, so we shrink the allocation.
		reallocateMemory(curr_size);
	}

	template<typename INT> reference operator[](INT index) {
		return (at(index));
	}

	template<typename INT> const_reference operator[](INT index) const {
		return (at(index));
	}

	template<typename INT> reference at(INT index) {
		static_assert(std::is_integral_v<INT>, "CString subscript operator index must be an integer.");

		if constexpr (std::is_unsigned_v<INT>) {
			return (data_ptr[getIndex(static_cast<size_type>(index))]);
		}
		else {
			return (data_ptr[getIndex(static_cast<difference_type>(index))]);
		}
	}

	template<typename INT> const_reference at(INT index) const {
		static_assert(std::is_integral_v<INT>, "CString subscript operator index must be an integer.");

		if constexpr (std::is_unsigned_v<INT>) {
			return (data_ptr[getIndex(static_cast<size_type>(index))]);
		}
		else {
			return (data_ptr[getIndex(static_cast<difference_type>(index))]);
		}
	}

	explicit operator std::basic_string_view<value_type>() const {
		return (std::basic_string_view<value_type>{c_str(), length()});
	}

	explicit operator std::basic_string<value_type>() const {
		return (std::basic_string<value_type>{c_str(), length()});
	}

	reference front() {
		return (at(0));
	}

	const_reference front() const {
		return (at(0));
	}

	reference back() {
		return (at(-1));
	}

	const_reference back() const {
		return (at(-1));
	}

	void push_back(value_type value) {
		ensureCapacity(curr_size + 1);

		data_ptr[curr_size] = value;

		curr_size++;
		nullTerminate();
	}

	void pop_back() {
		if (empty()) {
			throw std::out_of_range("string is empty.");
		}

		curr_size--;
		nullTerminate();
	}

	void clear() noexcept {
		curr_size = 0;
		nullTerminate();
	}

	void swap(basic_cstring &rhs) noexcept {
		std::swap(curr_size, rhs.curr_size);
		std::swap(maximum_size, rhs.maximum_size);
		std::swap(data_ptr, rhs.data_ptr);
	}

	// Iterator support.
	using iterator               = cPtrIterator<value_type>;
	using const_iterator         = cPtrIterator<const_value_type>;
	using reverse_iterator       = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	iterator begin() noexcept {
		// Empty container is a special case. front/back would throw.
		if (empty()) {
			return (iterator(data_ptr));
		}

		return (iterator(&front()));
	}

	iterator end() noexcept {
		// Empty container is a special case. front/back would throw.
		if (empty()) {
			return (iterator(data_ptr));
		}

		// Pointer must be to element one past the end!
		return (iterator((&back()) + 1));
	}

	const_iterator begin() const noexcept {
		return (cbegin());
	}

	const_iterator end() const noexcept {
		return (cend());
	}

	const_iterator cbegin() const noexcept {
		// Empty container is a special case. front/back would throw.
		if (empty()) {
			return (const_iterator(data_ptr));
		}

		return (const_iterator(&front()));
	}

	const_iterator cend() const noexcept {
		// Empty container is a special case. front/back would throw.
		if (empty()) {
			return (const_iterator(data_ptr));
		}

		// Pointer must be to element one past the end!
		return (const_iterator((&back()) + 1));
	}

	reverse_iterator rbegin() noexcept {
		return (reverse_iterator(end()));
	}

	reverse_iterator rend() noexcept {
		return (reverse_iterator(begin()));
	}

	const_reverse_iterator rbegin() const noexcept {
		return (crbegin());
	}

	const_reverse_iterator rend() const noexcept {
		return (crend());
	}

	const_reverse_iterator crbegin() const noexcept {
		return (const_reverse_iterator(cend()));
	}

	const_reverse_iterator crend() const noexcept {
		return (const_reverse_iterator(cbegin()));
	}

	iterator insert(const_iterator pos, value_type value) {
		return (insert(pos, 1, value));
	}

	iterator insert(const_iterator pos, size_type count, value_type value) {
		if (count == 0) {
			return (iterator::fromConst(pos));
		}

		// Careful: ensureCapacity() can invalidate iterators!
		// That's why we get the index first and regenerate pos.
		auto idx = static_cast<size_type>(std::distance(cbegin(), pos));
		ensureCapacity(curr_size + 1);
		auto wrPos = begin() + idx;

		// Move by N to make space.
		std::move_backward(wrPos, end(), end() + count);

		// Copy construct new elements at insertion position.
		std::fill_n(wrPos, count, value);

		curr_size += count;
		nullTerminate();

		return (wrPos);
	}

	template<class InputIt> iterator insert(const_iterator pos, InputIt first, InputIt last) {
		auto difference = std::distance(first, last);
		if (difference < 0) {
			throw std::invalid_argument("Inverted iterators (last < first). This is never what you really want.");
		}

		auto count = static_cast<size_type>(difference);
		if (count == 0) {
			return (iterator::fromConst(pos));
		}

		// Careful: ensureCapacity() can invalidate iterators!
		// That's why we get the index first and regenerate pos.
		auto idx = static_cast<size_type>(std::distance(cbegin(), pos));
		ensureCapacity(curr_size + 1);
		auto wrPos = begin() + idx;

		// Move by N to make space.
		std::move_backward(wrPos, end(), end() + count);

		// Copy construct new elements at insertion position from external range.
		std::copy_n(first, count, wrPos);

		curr_size += count;
		nullTerminate();

		return (wrPos);
	}

	iterator insert(const_iterator pos, std::initializer_list<value_type> init_list) {
		return (insert(pos, init_list.begin(), init_list.end()));
	}

	iterator erase(const_iterator pos) {
		auto wrPos = iterator::fromConst(pos);

		// Move elements over, this will move assign into the
		// to be erased element, effectively erasing it.
		std::move(wrPos + 1, end(), wrPos);

		// Destroy object at end, this was moved from and is
		// now waiting on destruction.
		curr_size--;
		nullTerminate();

		return (wrPos);
	}

	iterator erase(const_iterator first, const_iterator last) {
		auto wrFirst = iterator::fromConst(first);

		auto difference = std::distance(first, last);
		if (difference < 0) {
			throw std::invalid_argument("Inverted iterators (last < first). This is never what you really want.");
		}

		auto count = static_cast<size_type>(difference);
		if (count == 0) {
			return (wrFirst);
		}

		// Move elements over, this will move assign into the
		// to be erased element, effectively erasing it.
		std::move(iterator::fromConst(last), end(), wrFirst);

		// Destroy objects at end, they were moved from and are
		// now waiting on destruction.
		curr_size -= count;
		nullTerminate();

		return (wrFirst);
	}

	basic_cstring &append(const basic_cstring &str, size_type pos = 0, size_type count = npos) {
		return (append(str.c_str(), str.length(), pos, count));
	}

	basic_cstring &append(std::basic_string_view<value_type> str, size_type pos = 0, size_type count = npos) {
		return (append(str.data(), str.size(), pos, count));
	}

	basic_cstring &append(const_pointer str) {
		return (append(str, strlen(str)));
	}

	// Lowest common denominator: a ptr and sizes.
	basic_cstring &append(const_pointer str, size_type strLength, size_type pos = 0, size_type count = npos) {
		if (str == nullptr) {
			throw std::invalid_argument("string resolves to nullptr.");
		}

		if (pos > strLength) {
			throw std::length_error("position bigger than string length.");
		}

		// Ensure number of characters to copy is within range.
		if (count > (strLength - pos)) {
			count = strLength - pos;
		}

		ensureCapacity(curr_size + count);

		curr_size += count;

		std::copy_n(const_iterator(str + pos), count, end());
		nullTerminate();

		return (*this);
	}

	// Enlarge string with N times the given character.
	basic_cstring &append(size_type count, value_type value) {
		ensureCapacity(curr_size + count);

		curr_size += count;

		// Initialize elements to copy of value.
		std::fill_n(end(), count, value);
		nullTerminate();

		return (*this);
	}

	// Enlarge string with characters from range.
	template<class InputIt> basic_cstring &append(InputIt first, InputIt last) {
		auto difference = std::distance(first, last);
		if (difference < 0) {
			throw std::invalid_argument("Inverted iterators (last < first). This is never what you really want.");
		}

		auto count = static_cast<size_type>(difference);

		ensureCapacity(curr_size + count);

		curr_size += count;

		// Initialize elements to copy of range's values.
		std::copy_n(first, count, end());
		nullTerminate();

		return (*this);
	}

	// Enlarge string via initializer list {x, y, z}.
	basic_cstring &append(std::initializer_list<value_type> init_list) {
		return (append(init_list.begin(), init_list.end()));
	}

	basic_cstring &operator+=(const basic_cstring &rhs) {
		return (append(rhs));
	}

	basic_cstring &operator+=(std::basic_string_view<value_type> rhs) {
		return (append(rhs));
	}

	basic_cstring &operator+=(const_pointer rhs) {
		return (append(rhs));
	}

	basic_cstring &operator+=(value_type value) {
		return (append(1, value));
	}

	basic_cstring &operator+=(std::initializer_list<value_type> rhs_list) {
		return (append(rhs_list));
	}

	basic_cstring operator+(const basic_cstring &rhs) const {
		basic_cstring sum;
		sum.reserve(size() + rhs.size());

		sum.assign(*this);
		sum.append(rhs);

		return (sum);
	}

	basic_cstring operator+(std::basic_string_view<value_type> rhs) const {
		basic_cstring sum;
		sum.reserve(size() + rhs.size());

		sum.assign(*this);
		sum.append(rhs);

		return (sum);
	}

	friend basic_cstring operator+(std::basic_string_view<value_type> lhs, const basic_cstring &rhs) {
		return (rhs.operator+(lhs));
	}

	basic_cstring operator+(const_pointer rhs) const {
		size_type strLength = strlen(rhs);

		basic_cstring sum;
		sum.reserve(size() + strLength);

		sum.assign(*this);
		sum.append(rhs, strLength);

		return (sum);
	}

	friend basic_cstring operator+(const_pointer lhs, const basic_cstring &rhs) {
		return (rhs.operator+(lhs));
	}

	basic_cstring operator+(value_type value) const {
		basic_cstring sum;
		sum.reserve(size() + 1);

		sum.assign(*this);
		sum.append(1, value);

		return (sum);
	}

	friend basic_cstring operator+(value_type value, const basic_cstring &rhs) {
		return (rhs.operator+(value));
	}

	basic_cstring operator+(std::initializer_list<value_type> rhs_list) const {
		basic_cstring sum;
		sum.reserve(size() + rhs_list.size());

		sum.assign(*this);
		sum.append(rhs_list);

		return (sum);
	}

	friend basic_cstring operator+(std::initializer_list<value_type> lhs_list, const basic_cstring &rhs) {
		return (rhs.operator+(lhs_list));
	}

private:
	void nullTerminate() {
		data_ptr[curr_size] = 0;
	}

	void ensureCapacity(size_type newSize) {
		// Do we have enough space left?
		if (newSize <= maximum_size) {
			return; // Yes.
		}

		// No, we must grow.
		// Normally we try doubling the size, but we
		// have to check if even that is enough, and if
		// doubling violates the max_size() constraint.
		size_type double_max = maximum_size * 2;
		reallocateMemory(((double_max > newSize) && (double_max <= max_size())) ? (double_max) : (newSize));
	}

	void allocateMemory(size_type size) {
		data_ptr     = nullptr;
		maximum_size = 0;

		if (size > max_size()) {
			throw std::length_error("size exceeds max_size() limit.");
		}

		// Always allocate one byte more for NULL termination (C compatibility).
		data_ptr = static_cast<pointer>(malloc((size + 1) * sizeof(value_type)));
		if (data_ptr == nullptr) {
			// Failed.
			throw std::bad_alloc();
		}

		maximum_size = size;
	}

	void reallocateMemory(size_type newSize) {
		if (newSize > max_size()) {
			throw std::length_error("size exceeds max_size() limit.");
		}

		// Always allocate one byte more for NULL termination (C compatibility).
		pointer new_data_ptr = static_cast<pointer>(realloc(data_ptr, (newSize + 1) * sizeof(value_type)));
		if (new_data_ptr == nullptr) {
			// Failed.
			throw std::bad_alloc();
		}

		// Succeeded, update ptr + capacity.
		data_ptr     = new_data_ptr;
		maximum_size = newSize;
	}

	void freeMemory() {
		free(data_ptr);
		data_ptr     = nullptr;
		maximum_size = 0;
	}

	size_type getIndex(size_type index) const {
		if (index >= curr_size) {
			throw std::out_of_range("Index out of range.");
		}

		return (index);
	}

	size_type getIndex(difference_type index) const {
		// Support negative indexes to go from the last existing/defined element
		// backwards (not from the capacity!).
		if (index < 0) {
			index = static_cast<difference_type>(curr_size) + index;
		}

		if (index < 0 || static_cast<size_type>(index) >= curr_size) {
			throw std::out_of_range("Index out of range.");
		}

		return (static_cast<size_type>(index));
	}
};

using cstring    = basic_cstring<char>;
using cwstring   = basic_cstring<wchar_t>;
using cu16string = basic_cstring<char16_t>;
using cu32string = basic_cstring<char32_t>;

} // namespace dv

static_assert(std::is_standard_layout_v<dv::cstring>, "cstring is not standard layout");
static_assert(std::is_standard_layout_v<dv::cwstring>, "cwstring is not standard layout");
static_assert(std::is_standard_layout_v<dv::cu16string>, "cu16string is not standard layout");
static_assert(std::is_standard_layout_v<dv::cu32string>, "cu32string is not standard layout");

#endif // CSTRING_HPP
