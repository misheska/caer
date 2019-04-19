#ifndef DV_SDK_TRIGGER_HPP
#define DV_SDK_TRIGGER_HPP

#include "trigger_base.hpp"
#include "wrappers.hpp"

namespace dv {

template<> class InputWrapper<TriggerPacket> : public dv::cvectorConstProxy<TriggerT> {
private:
	using NativeType = typename TriggerPacket::NativeTableType;

	std::shared_ptr<const NativeType> ptr;

public:
	InputWrapper(std::shared_ptr<const NativeType> p) :
		dv::cvectorConstProxy<TriggerT>((p) ? (&p->triggers) : (nullptr)),
		ptr(std::move(p)) {
	}

	explicit operator bool() const noexcept {
		return (ptr.get() != nullptr);
	}

	std::shared_ptr<const NativeType> getBasePointer() const noexcept {
		return (ptr);
	}
};

template<> class OutputWrapper<TriggerPacket> : public dv::cvectorProxy<TriggerT> {
private:
	using NativeType = typename TriggerPacket::NativeTableType;

	NativeType *ptr;
	dvModuleData moduleData;
	std::string name;

public:
	OutputWrapper(NativeType *p, dvModuleData m, const std::string &n) :
		dv::cvectorProxy<TriggerT>((p) ? (&p->triggers) : (nullptr)),
		ptr(p),
		moduleData(m),
		name(n) {
	}

	void commit() noexcept {
		// Ignore empty trigger packets.
		if ((ptr == nullptr) || empty()) {
			return;
		}

		dvModuleOutputCommit(moduleData, name.c_str());

		// Update with next object, in case we continue to use this.
		auto typedObject = dvModuleOutputAllocate(moduleData, name.c_str());
		if (typedObject == nullptr) {
			// Actual errors will write a log message and return null.
			// No data just returns null. So if null we simply forward that.
			ptr = nullptr;
			reassign(nullptr);
		}
		else {
			ptr = static_cast<NativeType *>(typedObject->obj);
			reassign(&ptr->triggers);
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
};

} // namespace dv

#endif // DV_SDK_TRIGGER_HPP
