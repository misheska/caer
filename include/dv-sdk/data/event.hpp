#ifndef DV_SDK_EVENT_HPP
#define DV_SDK_EVENT_HPP

#include "event_base.hpp"
#include "wrappers.hpp"

// Allow disabling of OpenCV requirement.
#ifndef DV_API_OPENCV_SUPPORT
#	define DV_API_OPENCV_SUPPORT 1
#endif

#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
#	include <opencv2/core.hpp>
#	include <opencv2/core/utility.hpp>
#endif

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

	OutputDataWrapper<dv::EventPacket> &operator<<(const dv::Event &event) {
		push_back(event);
		return *this;
	}
};

/**
 * Describes an input for event packets. Offers convenience functions to obtain informations
 * about the event input as well as to get the event data.
 */
template<> class RuntimeInput<dv::EventPacket> : public _RuntimeInputCommon<dv::EventPacket> {
public:
	RuntimeInput(const std::string &name, dvModuleData moduleData) : _RuntimeInputCommon(name, moduleData) {
	}

	/**
	 * Returns an iterable container of the latest events that arrived at this input.
	 * @return An iterable container of the newest events.
	 */
	const InputDataWrapper<dv::EventPacket> events() const {
		return (data());
	}

	/**
	 * @return The width of the input region in pixels. Any event on this input will have a x-coordinate
	 * smaller than the return value of this function.
	 */
	int sizeX() const {
		return (infoNode().getInt("sizeX"));
	}

	/**
	 * @return The height of the input region in pixels. Any event on this input will have a y-coordinate
	 * smaller than the return value of this function
	 */
	int sizeY() const {
		return (infoNode().getInt("sizeY"));
	}

#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
	/**
	 * @return the input region size in pixels as an OpenCV size object
	 */
	const cv::Size size() const {
		return (cv::Size(sizeX(), sizeY()));
	}
#endif
};

struct Frame;

/**
 * Specialization of the runtime output for event outputs.
 * Provides convenience setup functions for setting up the event output
 */
template<> class RuntimeOutput<dv::EventPacket> : public _RuntimeOutputCommon<dv::EventPacket> {
public:
	RuntimeOutput(const std::string &name, dvModuleData moduleData) :
		_RuntimeOutputCommon<dv::EventPacket>(name, moduleData) {
	}

	OutputDataWrapper<dv::EventPacket> events() {
		return (data());
	}

	/**
	 * Sets up this event output by setting the provided arguments to the output info node
	 * @param sizeX The width of this event output
	 * @param sizeY The height of this event output
	 * @param originDescription A description that describes the original generator of the data
	 */
	void setup(int sizeX, int sizeY, const std::string &originDescription) {
		this->createSourceAttribute(originDescription);
		this->createSizeAttributes(sizeX, sizeY);
	}

	/**
	 * Sets this event output up with the same parameters as the supplied input.
	 * @param eventInput The event input to copy the information from
	 */
	void setup(const RuntimeInput<dv::EventPacket> &eventInput) {
		setup(eventInput.sizeX(), eventInput.sizeY(), eventInput.getOriginDescription());
	}

	/**
	 * Sets this event output up with the same parameters the the supplied input.
	 * @param frameInput  The frame input to copy the data from
	 */
	void setup(const RuntimeInput<dv::Frame> &frameInput);

	/**
	 * Convenience shorthand to commit an event directly onto this output.
	 * If not using this function, call `data` to get an output event packet
	 * to fill into.
	 * @param event The event to insert
	 * @return A reference to the this
	 */
	RuntimeOutput<dv::EventPacket> &operator<<(const dv::Event &event) {
		data() << event;
		return *this;
	}
};

} // namespace dv

#endif // DV_SDK_EVENT_HPP
