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

template<> class InputWrapper<IMUPacket> : public dv::cvectorConstProxy<IMUT> {
private:
	using NativeType = typename IMUPacket::NativeTableType;

	std::shared_ptr<const NativeType> ptr;

public:
	InputWrapper(std::shared_ptr<const NativeType> p) : dv::cvectorConstProxy<IMUT>(&p->samples), ptr(std::move(p)) {
	}

	std::shared_ptr<const NativeType> getBasePointer() {
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
		dv::cvectorProxy<IMUT>(&p->samples),
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

		reassign(&ptr->samples);
	}

	NativeType *getBasePointer() {
		return (ptr);
	}
};

template<> class InputWrapper<TriggerPacket> : public dv::cvectorConstProxy<TriggerT> {
private:
	using NativeType = typename TriggerPacket::NativeTableType;

	std::shared_ptr<const NativeType> ptr;

public:
	InputWrapper(std::shared_ptr<const NativeType> p) :
		dv::cvectorConstProxy<TriggerT>(&p->triggers),
		ptr(std::move(p)) {
	}

	std::shared_ptr<const NativeType> getBasePointer() {
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
		dv::cvectorProxy<TriggerT>(&p->triggers),
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

		reassign(&ptr->triggers);
	}

	NativeType *getBasePointer() {
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
		matPtr = std::shared_ptr<const cv::Mat>{new cv::Mat(ptr->sizeX, ptr->sizeY, static_cast<int>(ptr->format),
													const_cast<uint8_t *>(ptr->pixels.data())),
			[ptr = ptr](const cv::Mat *mp) { delete mp; }};
	}

	std::shared_ptr<const NativeType> getBasePointer() {
		return (ptr);
	}

	/**
	 * Return a read-only OpenCV Mat representing this frame.
	 *
	 * @return a read-only OpenCV Mat pointer
	 */
	std::shared_ptr<const cv::Mat> getMatPointer() {
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
};

} // namespace dv

#endif // DV_SDK_WRAPPERS_HPP
