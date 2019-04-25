#ifndef DV_SDK_MODULE_IO_HPP
#define DV_SDK_MODULE_IO_HPP

#include <dv-sdk/data/event.hpp>
#include <dv-sdk/data/frame_base.hpp>
#include <dv-sdk/data/imu.hpp>
#include <dv-sdk/data/trigger.hpp>
#include "data/wrappers.hpp"
#include "module.h"
#include "utils.h"

namespace dv {

class InputDefinition {
public:
	std::string name;
	std::string typeName;
	bool optional;

	InputDefinition(const std::string &n, const std::string &t, bool opt = false) :
		name(n),
		typeName(t),
		optional(opt) {
	}
};

class OutputDefinition {
public:
	std::string name;
	std::string typeName;

	OutputDefinition(const std::string &n, const std::string &t) : name(n), typeName(t) {
	}
};

class RuntimeInputs {
private:
	dvModuleData moduleData;

public:
	RuntimeInputs(dvModuleData m) : moduleData(m) {
	}

	template<typename T>
	std::shared_ptr<const typename T::NativeTableType> getUnwrapped(const std::string &name) const {
		auto typedObject = dvModuleInputGet(moduleData, name.c_str());
		if (typedObject == nullptr) {
			// Actual errors will write a log message and return null.
			// No data just returns null. So if null we simply forward that.
			return (nullptr);
		}

		// Build shared_ptr with custom deleter first, so that in verification failure case
		// (debug mode), memory gets properly cleaned up.
		std::shared_ptr<const typename T::NativeTableType> objPtr{
			static_cast<const typename T::NativeTableType *>(typedObject->obj),
			[moduleData = moduleData, name, typedObject](
				const typename T::NativeTableType *) { dvModuleInputDismiss(moduleData, name.c_str(), typedObject); }};

#ifndef NDEBUG
		if (typedObject->typeId != dvTypeIdentifierToId(T::identifier)) {
			throw std::runtime_error(
				"getUnwrapped(" + name + "): input type and given template type are not compatible.");
		}
#endif

		return (objPtr);
	}

	/**
	 * Get data from an input
	 * @tparam T The flatbuffers type of the data to be retrieved
	 * @param name The name of the input stream
	 * @return An input wrapper of the desired type, allowing data access
	 */
	template<typename T> const InputWrapper<T> get(const std::string &name) const {
		const InputWrapper<T> wrapper{getUnwrapped<T>(name)};
		return (wrapper);
	}

	/**
	 * (Convenience) Function to get events from an event input
	 * @param name the name of the event input stream
	 * @return An input wrapper of an event packet, allowing data access
	 */
	const InputWrapper<dv::EventPacket> getEvents(const std::string &name) const {
		return get<dv::EventPacket>(name);
	}

	/**
	 * (Convenience) Function to get a frame from a frame input
	 * @param name the name of the event input stream
	 * @return An input wrapper of a frame, allowing data access
	 */
	const InputWrapper<dv::Frame> getFrame(const std::string &name) const {
		return get<dv::Frame>(name);
	}

	/**
	 * (Convenience) Function to get IMU data from an IMU input
	 * @param name the name of the event input stream
	 * @return An input wrapper of an IMU packet, allowing data access
	 */
	const InputWrapper<dv::IMUPacket> getIMUMeasurements(const std::string &name) const {
		return get<dv::IMUPacket>(name);
	}

	/**
 	 * (Convenience) Function to get Trigger data from a Trigger input
 	 * @param name the name of the event input stream
 	 * @return An input wrapper of an Trigger packet, allowing data access
 	 */
	const InputWrapper<dv::TriggerPacket> getTriggers(const std::string &name) const {
		return get<dv::TriggerPacket>(name);
	}

	/**
	 * Returns an info node about the specified input. Can be used to determine dimensions of an
	 * input/output
	 * @param name The name of the input in question
	 * @return A node that contains the specified inputs information, such as "sizeX" or "sizeY"
	 */
	const dv::Config::Node getInfo(const std::string &name) const {
		// const_cast and then re-add const manually. Needed for transition to C++ type.
		return (const_cast<dvConfigNode>(dvModuleInputGetInfoNode(moduleData, name.c_str())));
	}

	bool isConnected(const std::string &name) const {
		return (dvModuleInputIsConnected(moduleData, name.c_str()));
	}
};

class RuntimeOutputs {
private:
	dvModuleData moduleData;

public:
	RuntimeOutputs(dvModuleData m) : moduleData(m) {
	}

	template<typename T> typename T::NativeTableType *allocateUnwrapped(const std::string &name) {
		auto typedObject = dvModuleOutputAllocate(moduleData, name.c_str());
		if (typedObject == nullptr) {
			// Actual errors will write a log message and return null.
			// No data just returns null. So if null we simply forward that.
			return (nullptr);
		}

#ifndef NDEBUG
		if (typedObject->typeId != dvTypeIdentifierToId(T::identifier)) {
			throw std::runtime_error(
				"allocateUnwrapped(" + name + "): output type and given template type are not compatible.");
		}
#endif

		return (static_cast<typename T::NativeTableType *>(typedObject->obj));
	}

	void commitUnwrapped(const std::string &name) {
		dvModuleOutputCommit(moduleData, name.c_str());
	}

	template<typename T> OutputWrapper<T> get(const std::string &name) {
		OutputWrapper<T> wrapper{allocateUnwrapped<T>(name), moduleData, name};
		return (wrapper);
	}

	/**
	 * (Convenience) Function to get events from an event output
	 * @param name the name of the event output stream
	 * @return An output wrapper of an event packet, allowing data access
	 */
	OutputWrapper<dv::EventPacket> getEvents(const std::string &name) {
		return get<dv::EventPacket>(name);
	}

	/**
	 * (Convenience) Function to get a frame from a frame output
	 * @param name the name of the event output stream
	 * @return An output wrapper of a frame, allowing data access
	 */
	OutputWrapper<dv::Frame> getFrame(const std::string &name) {
		return get<dv::Frame>(name);
	}

	/**
	 * (Convenience) Function to get IMU data from an IMU output
	 * @param name the name of the event output stream
	 * @return An output wrapper of an IMU packet, allowing data access
	 */
	OutputWrapper<dv::IMUPacket> getIMUMeasurements(const std::string &name) {
		return get<dv::IMUPacket>(name);
	}

	/**
 	 * (Convenience) Function to get Trigger data from a Trigger output
 	 * @param name the name of the event output stream
 	 * @return An output wrapper of an Trigger packet, allowing data access
 	 */
	OutputWrapper<dv::TriggerPacket> getTriggers(const std::string &name) {
		return get<dv::TriggerPacket>(name);
	}

	/**
	 * Returns an info node about the specified output. Can be used to determine dimensions of an
	 * input/output
	 * @param name The name of the output in question
	 * @return A node that contains the specified outputs information, such as "sizeX" or "sizeY"
	 */
	dv::Config::Node getInfo(const std::string &name) {
		return (dvModuleOutputGetInfoNode(moduleData, name.c_str()));
	}
};

} // namespace dv

#endif // DV_SDK_MODULE_IO_HPP
