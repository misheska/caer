#ifndef DV_SDK_WRAPPERS_HPP
#define DV_SDK_WRAPPERS_HPP

#include "../module.h"
#include "../utils.h"
#include "cvector_proxy.hpp"

namespace dv {

template<typename T> class InputDataWrapper {
private:
	using NativeType = typename T::NativeTableType;

	std::shared_ptr<const NativeType> ptr;

public:
	InputDataWrapper(std::shared_ptr<const NativeType> p) : ptr(std::move(p)) {
	}

	explicit operator bool() const noexcept {
		return (ptr.get() != nullptr);
	}

	std::shared_ptr<const NativeType> getBasePointer() const noexcept {
		return (ptr);
	}

	const NativeType &operator*() const noexcept {
		return (*(ptr.get()));
	}

	const NativeType *operator->() const noexcept {
		return (ptr.get());
	}
};

template<typename T> class OutputDataWrapper {
private:
	using NativeType = typename T::NativeTableType;

	NativeType *ptr;
	dvModuleData moduleData;
	std::string name;

public:
	OutputDataWrapper(NativeType *p, dvModuleData m, const std::string &n) : ptr(p), moduleData(m), name(n) {
	}

	void commit() noexcept {
		dvModuleOutputCommit(moduleData, name.c_str());

		// Update with next object, in case we continue to use this.
		auto typedObject = dvModuleOutputAllocate(moduleData, name.c_str());
		if (typedObject == nullptr) {
			// Actual errors will write a log message and return null.
			// No data just returns null. So if null we simply forward that.
			ptr = nullptr;
		}
		else {
			ptr = static_cast<NativeType *>(typedObject->obj);
		}
	}

	explicit operator bool() const noexcept {
		return (ptr != nullptr);
	}

	NativeType *getBasePointer() noexcept {
		return (ptr);
	}

	const NativeType *getBasePointer() const noexcept {
		return (ptr);
	}

	NativeType &operator*() noexcept {
		return (*ptr);
	}

	const NativeType &operator*() const noexcept {
		return (*ptr);
	}

	NativeType *operator->() noexcept {
		return (ptr);
	}

	const NativeType *operator->() const noexcept {
		return (ptr);
	}
};

} // namespace dv

#endif // DV_SDK_WRAPPERS_HPP
