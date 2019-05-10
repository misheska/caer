#ifndef DV_SDK_IMU_HPP
#define DV_SDK_IMU_HPP

#include "imu_base.hpp"
#include "wrappers.hpp"

namespace dv {

template<> class InputDataWrapper<IMUPacket> : public dv::cvectorConstProxy<IMUT> {
private:
	using NativeType = typename IMUPacket::NativeTableType;

	std::shared_ptr<const NativeType> ptr;

public:
	InputDataWrapper(std::shared_ptr<const NativeType> p) :
		dv::cvectorConstProxy<IMUT>((p) ? (&p->samples) : (nullptr)),
		ptr(std::move(p)) {
	}

	explicit operator bool() const noexcept {
		return (ptr.get() != nullptr);
	}

	std::shared_ptr<const NativeType> getBasePointer() const noexcept {
		return (ptr);
	}
};

template<> class OutputDataWrapper<IMUPacket> : public dv::cvectorProxy<IMUT> {
private:
	using NativeType = typename IMUPacket::NativeTableType;

	NativeType *ptr;
	dvModuleData moduleData;
	std::string name;

public:
	OutputDataWrapper(NativeType *p, dvModuleData m, const std::string &n) :
		dv::cvectorProxy<IMUT>((p) ? (&p->samples) : (nullptr)),
		ptr(p),
		moduleData(m),
		name(n) {
	}

	void commit() noexcept {
		// Ignore empty IMU packets.
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
			reassign(&ptr->samples);
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

#endif // DV_SDK_IMU_HPP
