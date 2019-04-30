#ifndef DV_SDK_EVENT_HPP
#define DV_SDK_EVENT_HPP

#include "event_base.hpp"
#include "wrappers.hpp"

namespace dv {

template<> class InputDataWrapper<EventPacket> : public dv::cvectorConstProxy<Event> {
private:
	using NativeType = typename EventPacket::NativeTableType;

	std::shared_ptr<const NativeType> ptr;

public:
	InputDataWrapper(std::shared_ptr<const NativeType> p) :
		dv::cvectorConstProxy<Event>((p) ? (&p->events) : (nullptr)),
		ptr(std::move(p)) {
	}

	explicit operator bool() const noexcept {
		return (ptr.get() != nullptr);
	}

	std::shared_ptr<const NativeType> getBasePointer() const noexcept {
		return (ptr);
	}
};

template<> class OutputDataWrapper<EventPacket> : public dv::cvectorProxy<Event> {
private:
	using NativeType = typename EventPacket::NativeTableType;

	NativeType *ptr;
	dvModuleData moduleData;
	std::string name;

public:
	OutputDataWrapper(NativeType *p, dvModuleData m, const std::string &n) :
		dv::cvectorProxy<Event>((p) ? (&p->events) : (nullptr)),
		ptr(p),
		moduleData(m),
		name(n) {
	}

	void commit() noexcept {
		// Ignore empty event packets.
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
			reassign(&ptr->events);
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


	OutputDataWrapper<dv::EventPacket>& operator<<(const dv ::Event &event) {
		push_back(event);
		return *this;
	}


};

} // namespace dv

#endif // DV_SDK_EVENT_HPP
