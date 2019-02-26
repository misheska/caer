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

	static_assert(std::is_standard_layout<T>::value, "cvector type is not standard layout");

private:
	pointer data_ptr;
	size_type curr_size;
	size_type max_size;

public:
	cvector() {
		curr_size = 0;
		max_size  = 128;
		data_ptr  = malloc(sizeof(T) * max_size);
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

	size_type capacity() const {
		return (max_size);
	}

	void resize(size_type newSize) {
	}

	reference operator[](size_type index) {
	}

	const_reference operator[](size_type index) const {
	}

	reference at(size_type index) {
	}

	const_reference at(size_type index) const {
	}
};

} // namespace dv

#endif // CVECTOR_HPP
