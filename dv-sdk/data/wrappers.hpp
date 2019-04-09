#ifndef DV_SDK_WRAPPERS_HPP
#define DV_SDK_WRAPPERS_HPP

#include "dv-sdk/utils.h"
#include "dv-sdk/module.h"

#include "cvector_proxy.hpp"
#include "event.hpp"

namespace dv {

template<typename T> class InputWrapper : public std::shared_ptr<const typename T::NativeTableType> {};

template<typename T> class OutputWrapper {
private:
	using NativeType = typename T::NativeTableType;

	NativeType *ptr;
	dvModuleData moduleData;
	std::string name;

public:
	OutputWrapper(NativeType *p, dvModuleData m, const std::string &n) : ptr(p), moduleData(m), name(n) {
	}

	void commit() {
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

	NativeType *getBasePointer() {
		return (ptr);
	}

	NativeType &operator*() const noexcept {
		return (*ptr);
	}

	NativeType *operator->() const noexcept {
		return (ptr);
	}
};

template<> class InputWrapper<EventPacket> : public dv::cvectorConstProxy<Event> {
private:
	using NativeType = typename EventPacket::NativeTableType;

	std::shared_ptr<const NativeType> ptr;

public:
	InputWrapper(std::shared_ptr<const NativeType> p) : dv::cvectorConstProxy<Event>(&p->events), ptr(std::move(p)) {
	}

	std::shared_ptr<const NativeType> getBasePointer() {
		return (ptr);
	}
};

template<> class OutputWrapper<EventPacket> : public dv::cvectorProxy<Event> {
private:
	using NativeType = typename EventPacket::NativeTableType;

	NativeType *ptr;
	dvModuleData moduleData;
	std::string name;

public:
	OutputWrapper(NativeType *p, dvModuleData m, const std::string &n) :
		dv::cvectorProxy<Event>(&p->events),
		ptr(p),
		moduleData(m),
		name(n) {
	}

	void commit() {
		// Ignore empty event packets.
		if (empty()) {
			return;
		}

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

		reassign(&ptr->events);
	}

	NativeType *getBasePointer() {
		return (ptr);
	}
};

} // namespace dv

#endif // DV_SDK_WRAPPERS_HPP
