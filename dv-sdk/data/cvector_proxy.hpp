#ifndef CVECTOR_PROXY_HPP
#define CVECTOR_PROXY_HPP

#include "cvector.hpp"

namespace dv {

template<class T> class cvectorConstProxy {
public:
	// Container traits.
	using value_type       = typename cvector<T>::value_type;
	using const_value_type = typename cvector<T>::const_value_type;
	using pointer          = typename cvector<T>::pointer;
	using const_pointer    = typename cvector<T>::const_pointer;
	using reference        = typename cvector<T>::reference;
	using const_reference  = typename cvector<T>::const_reference;
	using size_type        = typename cvector<T>::size_type;
	using difference_type  = typename cvector<T>::difference_type;

	static const size_type npos = cvector<T>::npos;

protected:
	cvector<T> *vec_ptr;

public:
	// C++ has no const constructors, so take care to restore const-ness call-side.
	cvectorConstProxy(const cvector<T> *vec) : vec_ptr(const_cast<cvector<T> *>(vec)) {
	}

	// Comparison operators.
	bool operator==(const cvectorConstProxy &rhs) const noexcept {
		return (vec_ptr->operator==(*rhs.vec_ptr));
	}

	bool operator!=(const cvectorConstProxy &rhs) const noexcept {
		return (vec_ptr->operator!=(*rhs.vec_ptr));
	}

	bool operator==(const cvector<value_type> &rhs) const noexcept {
		return (vec_ptr->operator==(rhs));
	}

	bool operator!=(const cvector<value_type> &rhs) const noexcept {
		return (vec_ptr->operator!=(rhs));
	}

	bool operator==(const std::vector<value_type> &rhs) const noexcept {
		return (vec_ptr->operator==(rhs));
	}

	bool operator!=(const std::vector<value_type> &rhs) const noexcept {
		return (vec_ptr->operator!=(rhs));
	}

	friend bool operator==(const cvector<value_type> &lhs, const cvectorConstProxy &rhs) noexcept {
		return (rhs.operator==(lhs));
	}

	friend bool operator!=(const cvector<value_type> &lhs, const cvectorConstProxy &rhs) noexcept {
		return (rhs.operator!=(lhs));
	}

	friend bool operator==(const std::vector<value_type> &lhs, const cvectorConstProxy &rhs) noexcept {
		return (rhs.operator==(lhs));
	}

	friend bool operator!=(const std::vector<value_type> &lhs, const cvectorConstProxy &rhs) noexcept {
		return (rhs.operator!=(lhs));
	}

	const_pointer data() const noexcept {
		return (vec_ptr->data());
	}

	size_type size() const noexcept {
		return (vec_ptr->size());
	}

	size_type capacity() const noexcept {
		return (vec_ptr->capacity());
	}

	size_type max_size() const noexcept {
		return (vec_ptr->max_size());
	}

	[[nodiscard]] bool empty() const noexcept {
		return (vec_ptr->empty());
	}

	template<typename INT> const_reference operator[](INT index) const {
		return (at(index));
	}

	template<typename INT> const_reference at(INT index) const {
		return (vec_ptr->at(index));
	}

	explicit operator std::vector<value_type>() const {
		return (std::vector<value_type>{cbegin(), cend()});
	}

	const_reference front() const {
		return (at(0));
	}

	const_reference back() const {
		return (at(-1));
	}

	// Iterator support.
	using iterator               = typename cvector<T>::iterator;
	using const_iterator         = typename cvector<T>::const_iterator;
	using reverse_iterator       = typename cvector<T>::reverse_iterator;
	using const_reverse_iterator = typename cvector<T>::const_reverse_iterator;

	const_iterator begin() const noexcept {
		return (cbegin());
	}

	const_iterator end() const noexcept {
		return (cend());
	}

	const_iterator cbegin() const noexcept {
		return (vec_ptr->cbegin());
	}

	const_iterator cend() const noexcept {
		return (vec_ptr->cend());
	}

	const_reverse_iterator rbegin() const noexcept {
		return (crbegin());
	}

	const_reverse_iterator rend() const noexcept {
		return (crend());
	}

	const_reverse_iterator crbegin() const noexcept {
		return (vec_ptr->crbegin());
	}

	const_reverse_iterator crend() const noexcept {
		return (vec_ptr->crend());
	}

	cvector<value_type> operator+(const cvectorConstProxy &rhs) const {
		return (vec_ptr->operator+(*rhs.vec_ptr));
	}

	cvector<value_type> operator+(const cvector<value_type> &rhs) const {
		return (vec_ptr->operator+(rhs));
	}

	cvector<value_type> operator+(const std::vector<value_type> &rhs) const {
		return (vec_ptr->operator+(rhs));
	}

	friend cvector<value_type> operator+(const cvector<value_type> &lhs, const cvectorConstProxy &rhs) {
		return (rhs.operator+(lhs));
	}

	friend cvector<value_type> operator+(const std::vector<value_type> &lhs, const cvectorConstProxy &rhs) {
		return (rhs.operator+(lhs));
	}

	cvector<value_type> operator+(const_reference value) const {
		return (vec_ptr->operator+(value));
	}

	friend cvector<value_type> operator+(const_reference value, const cvectorConstProxy &rhs) {
		return (rhs.operator+(value));
	}

	cvector<value_type> operator+(std::initializer_list<value_type> rhs_list) const {
		return (vec_ptr->operator+(rhs_list));
	}

	friend cvector<value_type> operator+(std::initializer_list<value_type> lhs_list, const cvectorConstProxy &rhs) {
		return (rhs.operator+(lhs_list));
	}
};

template<class T> class cvectorProxy : public cvectorConstProxy<T> {
public:
	// Container traits.
	using value_type       = typename cvector<T>::value_type;
	using const_value_type = typename cvector<T>::const_value_type;
	using pointer          = typename cvector<T>::pointer;
	using const_pointer    = typename cvector<T>::const_pointer;
	using reference        = typename cvector<T>::reference;
	using const_reference  = typename cvector<T>::const_reference;
	using size_type        = typename cvector<T>::size_type;
	using difference_type  = typename cvector<T>::difference_type;

	static const size_type npos = cvector<T>::npos;

	cvectorProxy(cvector<T> *vec) : cvectorConstProxy<T>(vec) {
	}

	void reassign(cvector<T> *vec) noexcept {
		this->vec_ptr = vec;
	}

	// Copy assignment.
	cvectorProxy &operator=(const cvectorProxy &rhs) {
		return (assign(rhs));
	}

	cvectorProxy &operator=(const cvector<value_type> &rhs) {
		return (assign(rhs));
	}

	// Extra assignment operators.
	cvectorProxy &operator=(const std::vector<value_type> &rhs) {
		return (assign(rhs));
	}

	cvectorProxy &operator=(const_reference value) {
		return (assign(1, value));
	}

	cvectorProxy &operator=(std::initializer_list<value_type> rhs_list) {
		return (assign(rhs_list));
	}

	cvectorProxy &assign(const cvectorProxy &vec, size_type pos = 0, size_type count = npos) {
		this->vec_ptr->assign(*vec.vec_ptr, pos, count);
		return (*this);
	}

	cvectorProxy &assign(const cvector<value_type> &vec, size_type pos = 0, size_type count = npos) {
		this->vec_ptr->assign(vec, pos, count);
		return (*this);
	}

	cvectorProxy &assign(const std::vector<value_type> &vec, size_type pos = 0, size_type count = npos) {
		this->vec_ptr->assign(vec, pos, count);
		return (*this);
	}

	// Lowest common denominator: a ptr and sizes. Most assignments call this.
	cvectorProxy &assign(const_pointer vec, size_type vecLength, size_type pos = 0, size_type count = npos) {
		this->vec_ptr->assign(vec, vecLength, pos, count);
		return (*this);
	}

	// Replace vector with N default constructed elements.
	cvectorProxy &assign(size_type count) {
		this->vec_ptr->assign(count);
		return (*this);
	}

	// Replace vector with N copies of given value.
	cvectorProxy &assign(size_type count, const_reference value) {
		this->vec_ptr->assign(count, value);
		return (*this);
	}

	// Replace vector with elements from range.
	template<class InputIt> cvectorProxy &assign(InputIt first, InputIt last) {
		this->vec_ptr->assign(first, last);
		return (*this);
	}

	// Replace vector via initializer list {x, y, z}.
	cvectorProxy &assign(std::initializer_list<value_type> init_list) {
		this->vec_ptr->assign(init_list);
		return (*this);
	}

	pointer data() noexcept {
		return (this->vec_ptr->data());
	}

	void resize(size_type newSize) {
		return (this->vec_ptr->resize(newSize));
	}

	void resize(size_type newSize, const_reference value) {
		return (this->vec_ptr->resize(newSize, value));
	}

	void reserve(size_type minCapacity) {
		return (this->vec_ptr->reserve(minCapacity));
	}

	void shrink_to_fit() {
		return (this->vec_ptr->shrink_to_fit());
	}

	template<typename INT> reference operator[](INT index) {
		return (at(index));
	}

	template<typename INT> reference at(INT index) {
		return (this->vec_ptr->at(index));
	}

	reference front() {
		return (at(0));
	}

	reference back() {
		return (at(-1));
	}

	void push_back(const_reference value) {
		this->vec_ptr->push_back(value);
	}

	void push_back(value_type &&value) {
		this->vec_ptr->push_back(value);
	}

	template<class... Args> void emplace_back(Args &&... args) {
		this->vec_ptr->emplace_back(std::forward<Args>(args)...);
	}

	void pop_back() {
		this->vec_ptr->pop_back();
	}

	void clear() noexcept {
		this->vec_ptr->clear();
	}

	void swap(cvectorProxy &rhs) noexcept {
		this->vec_ptr->swap(*rhs.vec_ptr);
	}

	void swap(cvector<value_type> &rhs) noexcept {
		this->vec_ptr->swap(rhs);
	}

	// Iterator support.
	using iterator               = typename cvector<T>::iterator;
	using const_iterator         = typename cvector<T>::const_iterator;
	using reverse_iterator       = typename cvector<T>::reverse_iterator;
	using const_reverse_iterator = typename cvector<T>::const_reverse_iterator;

	iterator begin() noexcept {
		return (this->vec_ptr->begin());
	}

	iterator end() noexcept {
		return (this->vec_ptr->end());
	}

	reverse_iterator rbegin() noexcept {
		return (this->vec_ptr->rbegin());
	}

	reverse_iterator rend() noexcept {
		return (this->vec_ptr->rend());
	}

	iterator insert(const_iterator pos, const_reference value) {
		return (this->vec_ptr->insert(pos, value));
	}

	iterator insert(const_iterator pos, value_type &&value) {
		return (this->vec_ptr->insert(pos, value));
	}

	iterator insert(const_iterator pos, size_type count, const_reference value) {
		return (this->vec_ptr->insert(pos, count, value));
	}

	template<class InputIt> iterator insert(const_iterator pos, InputIt first, InputIt last) {
		return (this->vec_ptr->insert(pos, first, last));
	}

	iterator insert(const_iterator pos, std::initializer_list<value_type> init_list) {
		return (this->vec_ptr->insert(pos, init_list));
	}

	iterator erase(const_iterator pos) {
		return (this->vec_ptr->erase(pos));
	}

	iterator erase(const_iterator first, const_iterator last) {
		return (this->vec_ptr->erase(first, last));
	}

	template<class... Args> iterator emplace(const_iterator pos, Args &&... args) {
		return (this->vec_ptr->emplace(pos, std::forward<Args>(args)...));
	}

	cvectorProxy &append(const cvectorProxy &vec, size_type pos = 0, size_type count = npos) {
		this->vec_ptr->append(*vec.vec_ptr, pos, count);
		return (*this);
	}

	cvectorProxy &append(const cvector<value_type> &vec, size_type pos = 0, size_type count = npos) {
		this->vec_ptr->append(vec, pos, count);
		return (*this);
	}

	cvectorProxy &append(const std::vector<value_type> &vec, size_type pos = 0, size_type count = npos) {
		this->vec_ptr->append(vec, pos, count);
		return (*this);
	}

	// Lowest common denominator: a ptr and sizes.
	cvectorProxy &append(const_pointer vec, size_type vecLength, size_type pos = 0, size_type count = npos) {
		this->vec_ptr->append(vec, vecLength, pos, count);
		return (*this);
	}

	// Enlarge vector with N default constructed elements.
	cvectorProxy &append(size_type count) {
		this->vec_ptr->append(count);
		return (*this);
	}

	// Enlarge vector with N copies of given value.
	cvectorProxy &append(size_type count, const_reference value) {
		this->vec_ptr->append(count, value);
		return (*this);
	}

	// Enlarge vector with elements from range.
	template<class InputIt> cvectorProxy &append(InputIt first, InputIt last) {
		this->vec_ptr->append(first, last);
		return (*this);
	}

	// Enlarge vector via initializer list {x, y, z}.
	cvectorProxy &append(std::initializer_list<value_type> init_list) {
		this->vec_ptr->append(init_list);
		return (*this);
	}

	cvectorProxy &operator+=(const cvectorProxy &rhs) {
		return (append(rhs));
	}

	cvectorProxy &operator+=(const cvector<value_type> &rhs) {
		return (append(rhs));
	}

	cvectorProxy &operator+=(const std::vector<value_type> &rhs) {
		return (append(rhs));
	}

	cvectorProxy &operator+=(const_reference value) {
		return (append(1, value));
	}

	cvectorProxy &operator+=(std::initializer_list<value_type> rhs_list) {
		return (append(rhs_list));
	}
};

} // namespace dv

#endif // CVECTOR_PROXY_HPP
