#ifndef DV_SDK_WRAPPERS_HPP
#define DV_SDK_WRAPPERS_HPP

#include "dv-sdk/module.h"
#include "dv-sdk/utils.h"

#include "cvector_proxy.hpp"
#include "event.hpp"
#include "frame.hpp"
#include "imu.hpp"
#include "trigger.hpp"

#include <opencv2/core.hpp>

namespace dv {

template<typename T> class InputWrapper {
private:
	using NativeType = typename T::NativeTableType;

	std::shared_ptr<const NativeType> ptr;

public:
	InputWrapper(std::shared_ptr<const NativeType> p) : ptr(std::move(p)) {
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

template<typename T> class OutputWrapper {
private:
	using NativeType = typename T::NativeTableType;

	NativeType *ptr;
	dvModuleData moduleData;
	std::string name;

public:
	OutputWrapper(NativeType *p, dvModuleData m, const std::string &n) : ptr(p), moduleData(m), name(n) {
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

template<> class InputWrapper<EventPacket> : public dv::cvectorConstProxy<Event> {
private:
	using NativeType = typename EventPacket::NativeTableType;

	std::shared_ptr<const NativeType> ptr;

public:
	InputWrapper(std::shared_ptr<const NativeType> p) :
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

template<> class OutputWrapper<EventPacket> : public dv::cvectorProxy<Event> {
private:
	using NativeType = typename EventPacket::NativeTableType;

	NativeType *ptr;
	dvModuleData moduleData;
	std::string name;

public:
	OutputWrapper(NativeType *p, dvModuleData m, const std::string &n) :
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
};

template<> class InputWrapper<IMUPacket> : public dv::cvectorConstProxy<IMUT> {
private:
	using NativeType = typename IMUPacket::NativeTableType;

	std::shared_ptr<const NativeType> ptr;

public:
	InputWrapper(std::shared_ptr<const NativeType> p) :
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

template<> class OutputWrapper<IMUPacket> : public dv::cvectorProxy<IMUT> {
private:
	using NativeType = typename IMUPacket::NativeTableType;

	NativeType *ptr;
	dvModuleData moduleData;
	std::string name;

public:
	OutputWrapper(NativeType *p, dvModuleData m, const std::string &n) :
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

template<> class InputWrapper<Frame> {
private:
	using NativeType = typename Frame::NativeTableType;

	std::shared_ptr<const NativeType> ptr;
	std::shared_ptr<const cv::Mat> matPtr;

public:
	InputWrapper(std::shared_ptr<const NativeType> p) : ptr(std::move(p)) {
		// Use custom deleter to bind life-time of main data 'ptr' to OpenCV 'matPtr'.
		if (ptr) {
			matPtr = std::shared_ptr<const cv::Mat>{new cv::Mat(ptr->sizeX, ptr->sizeY, static_cast<int>(ptr->format),
														const_cast<uint8_t *>(ptr->pixels.data())),
				[ptr = ptr](const cv::Mat *mp) { delete mp; }};
		}
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

	/**
	 * Return a read-only OpenCV Mat representing this frame.
	 *
	 * @return a read-only OpenCV Mat pointer
	 */
	std::shared_ptr<const cv::Mat> getMatPointer() const noexcept {
		return (matPtr);
	}
};

template<> class OutputWrapper<Frame> {
private:
	using NativeType = typename Frame::NativeTableType;

	NativeType *ptr;
	dvModuleData moduleData;
	std::string name;

public:
	OutputWrapper(NativeType *p, dvModuleData m, const std::string &n) : ptr(p), moduleData(m), name(n) {
	}

	void commit() noexcept {
		// Ignore frames with no pixels.
		if ((ptr == nullptr) || ptr->pixels.empty()) {
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
