#ifndef CSTRING_HPP
#define CSTRING_HPP

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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
	// Default constructor. Initialize empty string with space for 128 elements.
	cstring() {
		curr_size = 0;

		allocateMemory(128);
	}

	// Destructor.
	~cstring() noexcept {
		curr_size = 0;

		freeMemory();
	}

	// Initialize string with N times character.
	cstring(size_type count, const_value_type value) {
		curr_size = count;

		allocateMemory(count);

		// Initialize string with characters.
		for (size_type i = 0; i < curr_size; i++) {
			data_ptr[i] = value;
		}
	}

	// Initialize string with elements from range.
	template<class InputIt> cstring(InputIt first, InputIt last) {
		auto count = std::distance(first, last);

		curr_size = count;

		allocateMemory(count);

		// Initialize values to copy of range's values.
		for (size_type i = 0; i < curr_size; i++) {
			// Copy construct elements.
			new (&data_ptr[i]) T(*first);
			first++;
		}
	}

	// Initialize string via initializer list {x, y, z}.
	cstring(std::initializer_list<T> init_list) : cstring(init_list.begin(), init_list.end()) {
	}

	// Copy constructor.
	cstring(const cstring &rhs) {
		// Allocate memory for copy.
		allocateMemory(rhs.curr_size);

		// Full copy.
		curr_size = rhs.curr_size;

		for (size_type i = 0; i < curr_size; i++) {
			// Copy construct elements.
			new (&data_ptr[i]) T(rhs.data_ptr[i]);
		}
	}

	// Copy assignment.
	cstring &operator=(const cstring &rhs) {
		// If both the same, do nothing.
		if (this != &rhs) {
			// Destroy current data.
			destroyValues(0, curr_size);
			curr_size = 0;

			// Ensure space for new copied data.
			reallocateMemory(rhs.curr_size);

			// Full copy.
			curr_size = rhs.curr_size;

			for (size_type i = 0; i < curr_size; i++) {
				// Copy construct elements.
				new (&data_ptr[i]) T(rhs.data_ptr[i]);
			}
		}

		return (*this);
	}

	// Move constructor.
	cstring(cstring &&rhs) noexcept {
		// Moved-from object must remain in a valid state. We can define
		// valid-state-after-move to be nothing allowed but a destructor
		// call, which is what normally happens, and helps us a lot here.

		// Move data here.
		curr_size = rhs.curr_size;
		max_size  = rhs.max_size;
		data_ptr  = rhs.data_ptr;

		// Reset old data (ready for destruction).
		rhs.curr_size = 0;
		rhs.max_size  = 0;
		rhs.data_ptr  = nullptr;
	}

	// Move assignment.
	cstring &operator=(cstring &&rhs) noexcept {
		assert(this != &rhs);

		// Moved-from object must remain in a valid state. We can define
		// valid-state-after-move to be nothing allowed but a destructor
		// call, which is what normally happens, and helps us a lot here.

		// Destroy current data.
		destroyValues(0, curr_size);
		curr_size = 0;

		freeMemory();

		// Move data here.
		curr_size = rhs.curr_size;
		max_size  = rhs.max_size;
		data_ptr  = rhs.data_ptr;

		// Reset old data (ready for destruction).
		rhs.curr_size = 0;
		rhs.max_size  = 0;
		rhs.data_ptr  = nullptr;

		return (*this);
	}

	// Comparison operators.
	bool operator==(const cstring &rhs) const noexcept {
		if (curr_size != rhs.curr_size) {
			return (false);
		}

		for (size_type i = 0; i < curr_size; i++) {
			if (data_ptr[i] != rhs.data_ptr[i]) {
				return (false);
			}
		}

		return (true);
	}

	bool operator!=(const cstring &rhs) const noexcept {
		return (!operator==(rhs));
	}

	// Replace string with N default constructed elements.
	void assign(size_type count) {
		ensureCapacity(count);

		destroyValues(0, curr_size);

		curr_size = count;

		// Default initialize values.
		constructDefaultValues(0, curr_size);
	}

	// Replace string with N copies of given value.
	void assign(size_type count, const_reference value) {
		ensureCapacity(count);

		destroyValues(0, curr_size);

		curr_size = count;

		// Initialize values to copy of X.
		for (size_type i = 0; i < curr_size; i++) {
			// Copy construct elements.
			new (&data_ptr[i]) T(value);
		}
	}

	// Replace string with elements from range.
	template<class InputIt> void assign(InputIt first, InputIt last) {
		auto count = std::distance(first, last);

		ensureCapacity(count);

		destroyValues(0, curr_size);

		curr_size = count;

		// Initialize values to copy of range's values.
		for (size_type i = 0; i < curr_size; i++) {
			// Copy construct elements.
			new (&data_ptr[i]) T(*first);
			first++;
		}
	}

	// Replace string via initializer list {x, y, z}.
	void assign(std::initializer_list<T> init_list) {
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
		return (max_size);
	}

	bool empty() const noexcept {
		return (curr_size == 0);
	}

	void resize(size_type newSize) {
		ensureCapacity(newSize);

		if (newSize >= curr_size) {
			// Construct new values on expansion.
			constructDefaultValues(curr_size, newSize);
		}
		else {
			// Destroy on shrinking.
			destroyValues(newSize, curr_size);
		}

		curr_size = newSize;
	}

	void reserve(size_type minCapacity) {
		ensureCapacity(minCapacity);
	}

	void shrink_to_fit() {
		if (curr_size == max_size) {
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
		return (data_ptr[getIndex(static_cast<signed_size_type>(index))]);
	}

	const_reference at(int32_t index) const {
		return (data_ptr[getIndex(static_cast<signed_size_type>(index))]);
	}

	reference at(int64_t index) {
		return (data_ptr[getIndex(static_cast<signed_size_type>(index))]);
	}

	const_reference at(int64_t index) const {
		return (data_ptr[getIndex(static_cast<signed_size_type>(index))]);
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

		destroyValue(curr_size);
	}

	void clear() noexcept {
		destroyValues(0, curr_size);

		curr_size = 0;
	}

	void swap(cstring &rhs) noexcept {
		std::swap(curr_size, rhs.curr_size);
		std::swap(max_size, rhs.max_size);
		std::swap(data_ptr, rhs.data_ptr);
	}

	// Iterator support.
	using iterator               = cstringIterator<value_type>;
	using const_iterator         = cstringIterator<const_value_type>;
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
		auto idx = std::distance(cbegin(), pos);

		// Careful: this can invalidate iterators!
		// That's why we get the index above first.
		ensureCapacity(curr_size + 1);
		pos = cbegin() + idx;

		// Default construct so we can move into this.
		constructDefaultValue(curr_size);

		// Move by one to make space.
		std::move_backward(pos, cend(), cend() + 1);

		// Destroy object at insertion index.
		destroyValue(idx);

		// Move construct new element at insertion index.
		new (&data_ptr[idx]) T(std::move(value));

		curr_size++;

		return (pos);
	}

	iterator insert(const_iterator pos, size_type count, const_reference value) {
		if (count == 0) {
			return (pos);
		}

		auto idx = std::distance(cbegin(), pos);

		// Careful: this can invalidate iterators!
		// That's why we get the index above first.
		ensureCapacity(curr_size + count);
		pos = cbegin() + idx;

		// Default construct so we can move into this.
		constructDefaultValues(curr_size, curr_size + count);

		// Move by N to make space.
		std::move_backward(pos, cend(), cend() + count);

		// Destroy objects at insertion index.
		destroyValues(idx, idx + count);

		// Copy construct new elements at insertion index.
		for (size_type i = idx; i < (idx + count); i++) {
			new (&data_ptr[i]) T(value);
		}

		curr_size += count;

		return (pos);
	}

	template<class InputIt> iterator insert(const_iterator pos, InputIt first, InputIt last) {
		auto count = std::distance(first, last);
		if (count == 0) {
			return (pos);
		}

		auto idx = std::distance(cbegin(), pos);

		// Careful: this can invalidate iterators!
		// That's why we get the index above first.
		ensureCapacity(curr_size + count);
		pos = cbegin() + idx;

		// Default construct so we can move into this.
		constructDefaultValues(curr_size, curr_size + count);

		// Move by N to make space.
		std::move_backward(pos, cend(), cend() + count);

		// Destroy objects at insertion index.
		destroyValues(idx, idx + count);

		// Copy construct new elements at insertion index from external range.
		for (size_type i = idx; i < (idx + count); i++) {
			new (&data_ptr[i]) T(*first);
			first++;
		}

		curr_size += count;

		return (pos);
	}

	iterator insert(const_iterator pos, std::initializer_list<T> init_list) {
		return (insert(pos, init_list.begin(), init_list.end()));
	}

	iterator erase(const_iterator pos) {
		// Move elements over, this will move assign into the
		// to be erased element, effectively erasing it.
		std::move(pos + 1, cend(), pos);

		// Destroy object at end, this was moved from and is
		// now waiting on destruction.
		curr_size--;

		destroyValue(curr_size);

		return (pos);
	}

	iterator erase(const_iterator first, const_iterator last) {
		auto count = std::distance(first, last);
		if (count == 0) {
			return (first);
		}

		// Move elements over, this will move assign into the
		// to be erased element, effectively erasing it.
		std::move(last, cend(), first);

		// Destroy object at end, this was moved from and is
		// now waiting on destruction.
		curr_size -= count;

		destroyValue(curr_size, curr_size + count);

		return (first);
	}

	template<class... Args> iterator emplace(const_iterator pos, Args &&... args) {
		auto idx = std::distance(cbegin(), pos);

		// Careful: this can invalidate iterators!
		// That's why we get the index above first.
		ensureCapacity(curr_size + 1);
		pos = cbegin() + idx;

		// Default construct so we can move into this.
		constructDefaultValue(curr_size);

		// Move by one to make space.
		std::move_backward(pos, cend(), cend() + 1);

		// Destroy object at insertion index.
		destroyValue(idx);

		// Move construct new element at insertion index.
		new (&data_ptr[idx]) T(std::forward<Args>(args)...);

		curr_size++;

		return (pos);
	}

private:
	void constructDefaultValue(size_type index) {
		new (&data_ptr[index]) T();
	}

	void constructDefaultValues(size_type begin, size_type end) {
		for (size_type i = begin; i < end; i++) {
			constructDefaultValue(i);
		}
	}

	void destroyValue(size_type index) {
		data_ptr[index].~T();
	}

	void destroyValues(size_type begin, size_type end) {
		for (size_type i = begin; i < end; i++) {
			destroyValue(i);
		}
	}

	void ensureCapacity(size_type newSize) {
		// Do we have enough space left?
		if (newSize <= max_size) {
			return; // Yes.
		}

		// No, we must grow.
		// Normally we try doubling the size, but we
		// have to check if even that is enough.
		reallocateMemory(((max_size * 2) > newSize) ? (max_size * 2) : (newSize));
	}

	void allocateMemory(size_type size) {
		data_ptr = nullptr;

		if (size != 0) {
			data_ptr = malloc(size * sizeof(T));
			if (data_ptr == nullptr) {
				// Failed.
				throw std::bad_alloc();
			}
		}

		max_size = size;
	}

	void reallocateMemory(size_type newSize) {
		pointer new_data_ptr = nullptr;

		if constexpr (std::is_pod_v<T>) {
			// Type is POD, we can just use realloc.
			if (newSize != 0) {
				new_data_ptr = realloc(data_ptr, newSize * sizeof(T));
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
				new_data_ptr = malloc(newSize * sizeof(T));
				if (new_data_ptr == nullptr) {
					// Failed.
					throw std::bad_alloc();
				}

				// Move construct new memory.
				for (size_type i = 0; i < curr_size; i++) {
					new (&new_data_ptr[i]) T(std::move(data_ptr[i]));
				}
			}

			// Destroy old objects.
			destroyValues(0, curr_size);

			// Free old memory. Objects have been cleaned up above.
			free(data_ptr);
		}

		// Succeeded, update ptr + capacity.
		data_ptr = new_data_ptr;
		max_size = newSize;
	}

	void freeMemory() {
		free(data_ptr);
		data_ptr = nullptr;
		max_size = 0;
	}

	size_type getIndex(size_type index) const {
		if (index >= curr_size) {
			throw std::out_of_range("Index out of range.");
		}

		return (index);
	}

	size_type getIndex(signed_size_type index) const {
		// Support negative indexes to go from the last existing/defined element
		// backwards (not from the capacity!).
		if (index < 0) {
			index = curr_size + index;
		}

		if (index < 0 || index >= curr_size) {
			throw std::out_of_range("Index out of range.");
		}

		return (static_cast<size_type>(index));
	}
};

class cstring {
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
