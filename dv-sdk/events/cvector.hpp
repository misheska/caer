#ifndef CVECTOR_HPP
#define CVECTOR_HPP

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace dv {

template<class T> class cvectorIterator {
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
	cvectorIterator() : element_ptr(nullptr) {
	}

	cvectorIterator(pointer _element_ptr) : element_ptr(_element_ptr) {
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
	bool operator==(const cvectorIterator &rhs) const noexcept {
		return (element_ptr == rhs.element_ptr);
	}

	bool operator!=(const cvectorIterator &rhs) const noexcept {
		return (element_ptr != rhs.element_ptr);
	}

	bool operator<(const cvectorIterator &rhs) const noexcept {
		return (element_ptr < rhs.element_ptr);
	}

	bool operator>(const cvectorIterator &rhs) const noexcept {
		return (element_ptr > rhs.element_ptr);
	}

	bool operator<=(const cvectorIterator &rhs) const noexcept {
		return (element_ptr <= rhs.element_ptr);
	}

	bool operator>=(const cvectorIterator &rhs) const noexcept {
		return (element_ptr >= rhs.element_ptr);
	}

	// Prefix increment.
	cvectorIterator &operator++() noexcept {
		element_ptr++;
		return (*this);
	}

	// Postfix increment.
	cvectorIterator operator++(int) noexcept {
		return (cvectorIterator(element_ptr++));
	}

	// Prefix decrement.
	cvectorIterator &operator--() noexcept {
		element_ptr--;
		return (*this);
	}

	// Postfix decrement.
	cvectorIterator operator--(int) noexcept {
		return (cvectorIterator(element_ptr--));
	}

	// Iter += N.
	cvectorIterator &operator+=(size_type add) noexcept {
		element_ptr += add;
		return (*this);
	}

	// Iter + N.
	cvectorIterator operator+(size_type add) const noexcept {
		return (cvectorIterator(element_ptr + add));
	}

	// N + Iter. Must be friend as Iter is right-hand-side.
	friend cvectorIterator operator+(size_type lhs, const cvectorIterator &rhs) noexcept {
		return (cvectorIterator(rhs.element_ptr + lhs));
	}

	// Iter -= N.
	cvectorIterator &operator-=(size_type sub) noexcept {
		element_ptr -= sub;
		return (*this);
	}

	// Iter - N. (N - Iter doesn't make sense!)
	cvectorIterator operator-(size_type sub) const noexcept {
		return (cvectorIterator(element_ptr - sub));
	}

	// Iter - Iter. (Iter + Iter doesn't make sense!)
	difference_type operator-(const cvectorIterator &rhs) const noexcept {
		return (element_ptr - rhs.element_ptr);
	}

	// Swap two iterators.
	void swap(cvectorIterator &rhs) noexcept {
		std::swap(element_ptr, rhs.element_ptr);
	}
};

template<class T> class cvector {
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

	static_assert(std::is_standard_layout_v<value_type>, "cvector type is not standard layout");

private:
	size_type curr_size;
	size_type maximum_size;
	pointer data_ptr;

public:
	// Default constructor. Initialize empty vector with space for 128 elements.
	cvector() {
		curr_size = 0;

		allocateMemory(128);
	}

	// Destructor.
	~cvector() noexcept {
		// Destroy all elements.
		std::destroy_n(begin(), curr_size);
		curr_size = 0;

		freeMemory();
	}

	// Copy constructor.
	cvector(const cvector &vec) : cvector(vec, 0) {
	}

	cvector(const cvector &vec, size_type pos) : cvector(vec, pos, npos) {
	}

	cvector(const cvector &vec, size_type pos, size_type count) : cvector(vec.data(), vec.size(), pos, count) {
	}

	cvector(const std::vector<value_type> &vec) : cvector(vec, 0) {
	}

	cvector(const std::vector<value_type> &vec, size_type pos) : cvector(vec, pos, npos) {
	}

	cvector(const std::vector<value_type> &vec, size_type pos, size_type count) :
		cvector(vec.data(), vec.size(), pos, count) {
	}

	cvector(const_pointer vec, size_type vecLength) : cvector(vec, vecLength, 0) {
	}

	cvector(const_pointer vec, size_type vecLength, size_type pos) : cvector(vec, vecLength, pos, npos) {
	}

	// Lowest common denominator: a ptr and sizes. Most constructors call this.
	cvector(const_pointer vec, size_type vecLength, size_type pos, size_type count) {
		if (vec == nullptr) {
			throw std::invalid_argument("vector resolves to nullptr.");
		}

		if (pos > vecLength) {
			throw std::length_error("position bigger than vector length.");
		}

		// Ensure number of elements to copy is within range.
		if (count > (vecLength - pos)) {
			count = vecLength - pos;
		}

		curr_size = count;

		allocateMemory(count);

		std::uninitialized_copy_n(const_iterator(vec + pos), count, begin());
	}

	// Initialize vector with N default constructed elements.
	cvector(size_type count) {
		curr_size = count;

		allocateMemory(count);

		// Default initialize elements.
		std::uninitialized_default_construct_n(begin(), count);
	}

	// Initialize vector with N copies of given value.
	cvector(size_type count, const_reference value) {
		curr_size = count;

		allocateMemory(count);

		// Initialize elements to copy of value.
		std::uninitialized_fill_n(begin(), count, value);
	}

	// Initialize vector with elements from range.
	template<class InputIt> cvector(InputIt first, InputIt last) {
		auto difference = std::distance(first, last);
		if (difference < 0) {
			throw std::invalid_argument("Inverted iterators (last < first). This is never what you really want.");
		}

		auto count = static_cast<size_type>(std::abs(difference));

		curr_size = count;

		allocateMemory(count);

		// Initialize elements to copy of range's values.
		std::uninitialized_copy_n(first, count, begin());
	}

	// Initialize vector via initializer list {x, y, z}.
	cvector(std::initializer_list<value_type> init_list) : cvector(init_list.begin(), init_list.end()) {
	}

	// Move constructor.
	cvector(cvector &&rhs) noexcept {
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
	cvector &operator=(cvector &&rhs) noexcept {
		assign(std::move(rhs));

		return (*this);
	}

	// Copy assignment.
	cvector &operator=(const cvector &rhs) {
		assign(rhs);

		return (*this);
	}

	// Extra assignment operators.
	cvector &operator=(const std::vector<value_type> &rhs) {
		assign(rhs);

		return (*this);
	}

	cvector &operator=(const_reference value) {
		assign(1, value);

		return (*this);
	}

	cvector &operator=(std::initializer_list<value_type> rhs_list) {
		assign(rhs_list);

		return (*this);
	}

	// Comparison operators.
	bool operator==(const cvector &rhs) const noexcept {
		return (std::equal(cbegin(), cend(), rhs.cbegin(), rhs.cend()));
	}

	bool operator!=(const cvector &rhs) const noexcept {
		return (!operator==(rhs));
	}

	void assign(cvector &&vec) noexcept {
		assert(this != &vec);

		// Moved-from object must remain in a valid state. We can define
		// valid-state-after-move to be nothing allowed but a destructor
		// call, which is what normally happens, and helps us a lot here.

		// Destroy current data.
		std::destroy_n(begin(), curr_size);
		curr_size = 0;

		freeMemory();

		// Move data here.
		curr_size    = vec.curr_size;
		maximum_size = vec.maximum_size;
		data_ptr     = vec.data_ptr;

		// Reset old data (ready for destruction).
		vec.curr_size    = 0;
		vec.maximum_size = 0;
		vec.data_ptr     = nullptr;
	}

	void assign(const cvector &vec) {
		// If both the same, do nothing.
		if (this != &vec) {
			assign(vec, 0);
		}
	}

	void assign(const cvector &vec, size_type pos) {
		assign(vec, pos, npos);
	}

	void assign(const cvector &vec, size_type pos, size_type count) {
		assign(vec.data(), vec.size(), pos, count);
	}

	void assign(const std::vector<value_type> &vec) {
		assign(vec, 0);
	}

	void assign(const std::vector<value_type> &vec, size_type pos) {
		assign(vec, pos, npos);
	}

	void assign(const std::vector<value_type> &vec, size_type pos, size_type count) {
		assign(vec.data(), vec.size(), pos, count);
	}

	void assign(const_pointer vec, size_type vecLength) {
		assign(vec, vecLength, 0);
	}

	void assign(const_pointer vec, size_type vecLength, size_type pos) {
		assign(vec, vecLength, pos, npos);
	}

	// Lowest common denominator: a ptr and sizes. Most assignments call this.
	void assign(const_pointer vec, size_type vecLength, size_type pos, size_type count) {
		if (vec == nullptr) {
			throw std::invalid_argument("vector resolves to nullptr.");
		}

		if (pos > vecLength) {
			throw std::length_error("position bigger than vector length.");
		}

		// Ensure number of elements to copy is within range.
		if (count > (vecLength - pos)) {
			count = vecLength - pos;
		}

		ensureCapacity(count);

		std::destroy_n(begin(), curr_size);

		curr_size = count;

		std::uninitialized_copy_n(const_iterator(vec + pos), count, begin());
	}

	// Replace vector with N default constructed elements.
	void assign(size_type count) {
		ensureCapacity(count);

		std::destroy_n(begin(), curr_size);

		curr_size = count;

		// Default initialize elements.
		std::uninitialized_default_construct_n(begin(), count);
	}

	// Replace vector with N copies of given value.
	void assign(size_type count, const_reference value) {
		ensureCapacity(count);

		std::destroy_n(begin(), curr_size);

		curr_size = count;

		// Initialize elements to copy of value.
		std::uninitialized_fill_n(begin(), count, value);
	}

	// Replace vector with elements from range.
	template<class InputIt> void assign(InputIt first, InputIt last) {
		auto difference = std::distance(first, last);
		if (difference < 0) {
			throw std::invalid_argument("Inverted iterators (last < first). This is never what you really want.");
		}

		auto count = static_cast<size_type>(std::abs(difference));

		ensureCapacity(count);

		std::destroy_n(begin(), curr_size);

		curr_size = count;

		// Initialize elements to copy of range's values.
		std::uninitialized_copy_n(first, count, begin());
	}

	// Replace vector via initializer list {x, y, z}.
	void assign(std::initializer_list<value_type> init_list) {
		assign(init_list.begin(), init_list.end());
	}

	pointer data() noexcept {
		return (data_ptr);
	}

	const_pointer data() const noexcept {
		return (data_ptr);
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
		return (static_cast<size_type>(std::numeric_limits<difference_type>::max()));
	}

	[[nodiscard]] bool empty() const noexcept {
		return (curr_size == 0);
	}

	void resize(size_type newSize) {
		ensureCapacity(newSize);

		if (newSize >= curr_size) {
			// Construct new values on expansion.
			std::uninitialized_default_construct_n(end(), (newSize - curr_size));
		}
		else {
			// Destroy on shrinking.
			std::destroy_n(begin() + newSize, (curr_size - newSize));
		}

		curr_size = newSize;
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

	reference operator[](int32_t index) {
		return (at(index));
	}

	const_reference operator[](int32_t index) const {
		return (at(index));
	}

	reference operator[](int64_t index) {
		return (at(index));
	}

	const_reference operator[](int64_t index) const {
		return (at(index));
	}

	reference operator[](uint32_t index) {
		return (at(index));
	}

	const_reference operator[](uint32_t index) const {
		return (at(index));
	}

	reference operator[](uint64_t index) {
		return (at(index));
	}

	const_reference operator[](uint64_t index) const {
		return (at(index));
	}

	reference at(int32_t index) {
		return (data_ptr[getIndex(static_cast<difference_type>(index))]);
	}

	const_reference at(int32_t index) const {
		return (data_ptr[getIndex(static_cast<difference_type>(index))]);
	}

	reference at(int64_t index) {
		return (data_ptr[getIndex(static_cast<difference_type>(index))]);
	}

	const_reference at(int64_t index) const {
		return (data_ptr[getIndex(static_cast<difference_type>(index))]);
	}

	reference at(uint32_t index) {
		return (data_ptr[getIndex(static_cast<size_type>(index))]);
	}

	const_reference at(uint32_t index) const {
		return (data_ptr[getIndex(static_cast<size_type>(index))]);
	}

	reference at(uint64_t index) {
		return (data_ptr[getIndex(static_cast<size_type>(index))]);
	}

	const_reference at(uint64_t index) const {
		return (data_ptr[getIndex(static_cast<size_type>(index))]);
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

	void push_back(const_reference value) {
		ensureCapacity(curr_size + 1);

		// Call copy constructor.
		new (&data_ptr[curr_size]) T(value);

		curr_size++;
	}

	void push_back(value_type &&value) {
		ensureCapacity(curr_size + 1);

		// Call move constructor.
		new (&data_ptr[curr_size]) T(std::move(value));

		curr_size++;
	}

	template<class... Args> void emplace_back(Args &&... args) {
		ensureCapacity(curr_size + 1);

		// Call constructor with forwarded arguments.
		new (&data_ptr[curr_size]) T(std::forward<Args>(args)...);

		curr_size++;
	}

	void pop_back() noexcept {
		curr_size--;

		std::destroy_n(end(), 1);
	}

	void clear() noexcept {
		std::destroy_n(begin(), curr_size);

		curr_size = 0;
	}

	void swap(cvector &rhs) noexcept {
		std::swap(curr_size, rhs.curr_size);
		std::swap(maximum_size, rhs.maximum_size);
		std::swap(data_ptr, rhs.data_ptr);
	}

	// Iterator support.
	using iterator               = cvectorIterator<value_type>;
	using const_iterator         = cvectorIterator<const_value_type>;
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

	iterator insert(const_iterator pos, const_reference value) {
		return (insert(pos, 1, value));
	}

	iterator insert(const_iterator pos, T &&value) {
		// Careful: ensureCapacity() can invalidate iterators!
		// That's why we get the index first and regenerate pos.
		auto idx = std::distance(cbegin(), pos);
		ensureCapacity(curr_size + 1);
		pos = cbegin() + idx;

		// Default construct so we can move into this.
		std::uninitialized_default_construct_n(end(), 1);

		// Move by one to make space.
		std::move_backward(pos, cend(), end() + 1);

		// Destroy object at insertion position.
		std::destroy_n(pos, 1);

		// Move construct new element at insertion index.
		new (&data_ptr[idx]) T(std::move(value));

		curr_size++;

		return (pos);
	}

	iterator insert(const_iterator pos, size_type count, const_reference value) {
		if (count == 0) {
			return (pos);
		}

		// Careful: ensureCapacity() can invalidate iterators!
		// That's why we get the index first and regenerate pos.
		auto idx = std::distance(cbegin(), pos);
		ensureCapacity(curr_size + 1);
		pos = cbegin() + idx;

		// Default construct so we can move into this.
		std::uninitialized_default_construct_n(end(), count);

		// Move by N to make space.
		std::move_backward(pos, cend(), end() + count);

		// Destroy objects at insertion position.
		std::destroy_n(pos, count);

		// Copy construct new elements at insertion position.
		std::uninitialized_fill_n(pos, count, value);

		curr_size += count;

		return (pos);
	}

	template<class InputIt> iterator insert(const_iterator pos, InputIt first, InputIt last) {
		auto difference = std::distance(first, last);
		if (difference < 0) {
			throw std::invalid_argument("Inverted iterators (last < first). This is never what you really want.");
		}

		auto count = static_cast<size_type>(std::abs(difference));
		if (count == 0) {
			return (pos);
		}

		// Careful: ensureCapacity() can invalidate iterators!
		// That's why we get the index first and regenerate pos.
		auto idx = std::distance(cbegin(), pos);
		ensureCapacity(curr_size + 1);
		pos = cbegin() + idx;

		// Default construct so we can move into this.
		std::uninitialized_default_construct_n(end(), count);

		// Move by N to make space.
		std::move_backward(pos, cend(), end() + count);

		// Destroy objects at insertion position.
		std::destroy_n(pos, count);

		// Copy construct new elements at insertion position from external range.
		std::uninitialized_copy_n(first, count, pos);

		curr_size += count;

		return (pos);
	}

	iterator insert(const_iterator pos, std::initializer_list<value_type> init_list) {
		return (insert(pos, init_list.begin(), init_list.end()));
	}

	iterator erase(const_iterator pos) {
		// Move elements over, this will move assign into the
		// to be erased element, effectively erasing it.
		std::move(pos + 1, cend(), pos);

		// Destroy object at end, this was moved from and is
		// now waiting on destruction.
		curr_size--;

		std::destroy_n(end(), 1);

		return (pos);
	}

	iterator erase(const_iterator first, const_iterator last) {
		auto difference = std::distance(first, last);
		if (difference < 0) {
			throw std::invalid_argument("Inverted iterators (last < first). This is never what you really want.");
		}

		auto count = static_cast<size_type>(std::abs(difference));
		if (count == 0) {
			return (first);
		}

		// Move elements over, this will move assign into the
		// to be erased element, effectively erasing it.
		std::move(last, cend(), first);

		// Destroy objects at end, they were moved from and are
		// now waiting on destruction.
		curr_size -= count;

		std::destroy_n(end(), count);

		return (first);
	}

	template<class... Args> iterator emplace(const_iterator pos, Args &&... args) {
		// Careful: ensureCapacity() can invalidate iterators!
		// That's why we get the index first and regenerate pos.
		auto idx = std::distance(cbegin(), pos);
		ensureCapacity(curr_size + 1);
		pos = cbegin() + idx;

		// Default construct so we can move into this.
		std::uninitialized_default_construct_n(end(), 1);

		// Move by one to make space.
		std::move_backward(pos, cend(), end() + 1);

		// Destroy object at insertion position.
		std::destroy_n(pos, 1);

		// Move construct new element at insertion index.
		new (&data_ptr[idx]) T(std::forward<Args>(args)...);

		curr_size++;

		return (pos);
	}

private:
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

		if (size != 0) {
			data_ptr = static_cast<pointer>(malloc(size * sizeof(T)));
			if (data_ptr == nullptr) {
				// Failed.
				throw std::bad_alloc();
			}

			maximum_size = size;
		}
	}

	void reallocateMemory(size_type newSize) {
		if (newSize > max_size()) {
			throw std::length_error("size exceeds max_size() limit.");
		}

		pointer new_data_ptr = nullptr;

		if constexpr (std::is_pod_v<value_type>) {
			// Type is POD, we can just use realloc.
			if (newSize != 0) {
				new_data_ptr = static_cast<pointer>(realloc(data_ptr, newSize * sizeof(T)));
				if (new_data_ptr == nullptr) {
					// Failed.
					throw std::bad_alloc();
				}
			}
			else {
				// Like realloc(x, 0), but well-defined.
				free(data_ptr);
			}
		}
		else {
			// Type is not POD (C++ object), we cannot use realloc directly.
			// So we malloc the new size, move objects over, and then free.
			if (newSize != 0) {
				new_data_ptr = static_cast<pointer>(malloc(newSize * sizeof(T)));
				if (new_data_ptr == nullptr) {
					// Failed.
					throw std::bad_alloc();
				}

				// Move construct new memory.
				std::uninitialized_move_n(begin(), curr_size, iterator(new_data_ptr));
			}

			// Destroy old objects.
			std::destroy_n(begin(), curr_size);

			// Free old memory. Objects have been cleaned up above.
			free(data_ptr);
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

} // namespace dv

static_assert(std::is_standard_layout_v<dv::cvector<int>>, "cvector itself is not standard layout");

#endif // CVECTOR_HPP
