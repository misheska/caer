#ifndef CVECTOR_HPP
#define CVECTOR_HPP

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <type_traits>

namespace dv {

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

	static_assert(std::is_standard_layout_v<T>, "cvector type is not standard layout");

private:
	pointer data_ptr;
	size_type curr_size;
	size_type max_size;

public:
	cvector() {
		curr_size = 0;
		max_size  = 128;
		data_ptr  = malloc(max_size * sizeof(T));
	}

	~cvector() {
		// Destroy all values.
		destroyValues(0, curr_size);

		free(data_ptr);
	}

	pointer data() {
		return (data_ptr);
	}

	const_pointer data() const {
		return (data_ptr);
	}

	size_type size() const {
		return (curr_size);
	}

	size_type length() const {
		return (size());
	}

	size_type capacity() const {
		return (max_size);
	}

	bool empty() const {
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
		pointer new_data_ptr = realloc(data_ptr, curr_size * sizeof(T));
		if (new_data_ptr == nullptr) {
			// Failed.
			throw std::bad_alloc();
		}

		// Succeeded, update ptr + capacity.
		data_ptr = new_data_ptr;
		max_size = curr_size;
	}

	reference operator[](size_type index) {
		return (data_ptr[getIndex(index)]);
	}

	const_reference operator[](size_type index) const {
		return (data_ptr[getIndex(index)]);
	}

	reference at(size_type index) {
		return (data_ptr[getIndex(index)]);
	}

	const_reference at(size_type index) const {
		return (data_ptr[getIndex(index)]);
	}

	reference front() {
		return (data_ptr[getIndex(0)]);
	}

	const_reference front() const {
		return (data_ptr[getIndex(0)]);
	}

	reference back() {
		return (data_ptr[getIndex(-1)]);
	}

	const_reference back() const {
		return (data_ptr[getIndex(-1)]);
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

	void pop_back() {
		curr_size--;

		destroyValue(curr_size);
	}

	void clear() {
		destroyValues(0, curr_size);

		curr_size = 0;
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

		// No, realloc is needed.
		pointer new_data_ptr = realloc(data_ptr, max_size * 2 * sizeof(T));
		if (new_data_ptr == nullptr) {
			// Failed.
			throw std::bad_alloc();
		}

		// Succeeded, update ptr + capacity.
		data_ptr = new_data_ptr;
		max_size *= 2;
	}

	size_type getIndex(size_type index) const {
		// Support negative indexes to go from the last existing/defined element
		// backwards (not from the capacity!).
		if (index < 0) {
			index = curr_size + index;
		}

		if (index < 0 || index >= curr_size) {
			throw std::out_of_range("Index out of range.");
		}

		return (index);
	}
};

} // namespace dv

static_assert(std::is_standard_layout_v<dv::cvector<int>>, "cvector itself is not standard layout");

#endif // CVECTOR_HPP
