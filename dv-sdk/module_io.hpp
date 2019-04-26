#ifndef DV_SDK_MODULE_IO_HPP
#define DV_SDK_MODULE_IO_HPP

#include <dv-sdk/data/event.hpp>
#include <dv-sdk/data/frame.hpp>
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


template <typename T>
class RuntimeInputCommon {
private:
	std::string name_;
	dvModuleData moduleData_;

    std::shared_ptr<const typename T::NativeTableType> getUnwrapped(const std::string &name) const {
        auto typedObject = dvModuleInputGet(moduleData_, name.c_str());
        if (typedObject == nullptr) {
            // Actual errors will write a log message and return null.
            // No data just returns null. So if null we simply forward that.
            return (nullptr);
        }

        // Build shared_ptr with custom deleter first, so that in verification failure case
        // (debug mode), memory gets properly cleaned up.
        std::shared_ptr<const typename T::NativeTableType> objPtr{
                static_cast<const typename T::NativeTableType *>(typedObject->obj),
                [moduleData = moduleData_, name, typedObject](
                        const typename T::NativeTableType *) { dvModuleInputDismiss(moduleData, name.c_str(), typedObject); }};

#ifndef NDEBUG
        if (typedObject->typeId != dvTypeIdentifierToId(T::identifier)) {
            throw std::runtime_error(
                    "getUnwrapped(" + name + "): input type and given template type are not compatible.");
        }
#endif

        return (objPtr);
    }


public:
	RuntimeInputCommon(const std::string &name, dvModuleData moduleData) : name_(name), moduleData_(moduleData) {
	}

	/**
	 * Get data from an input
	 * @param name The name of the input stream
	 * @return An input wrapper of the desired type, allowing data access
	 */
	const InputDataWrapper<T> data() const {
		const InputDataWrapper<T> wrapper{getUnwrapped(name_)};
		return (wrapper);
	}

	/**
	 * Returns an info node about the specified input. Can be used to determine dimensions of an
	 * input/output
	 * @return A node that contains the specified inputs information, such as "sizeX" or "sizeY"
	 */
	const dv::Config::Node info() const {
		// const_cast and then re-add const manually. Needed for transition to C++ type.
		return (const_cast<dvConfigNode>(dvModuleInputGetInfoNode(moduleData_, name_.c_str())));
	}

	bool isConnected() const {
		return (dvModuleInputIsConnected(moduleData_, name_.c_str()));
	}

};

template <typename T>
class RuntimeInput : public RuntimeInputCommon<T> {
public:
	RuntimeInput<T>(const std::string &name, dvModuleData moduleData) : RuntimeInputCommon<T>(name, moduleData) {
	}
};

template <>
class RuntimeInput<dv::EventPacket> : public RuntimeInputCommon<dv::EventPacket> {
public:
	RuntimeInput(const std::string &name, dvModuleData moduleData) : RuntimeInputCommon(name, moduleData) {
	}

	int sizeX() const {
		return info().getInt("sizeX");
	}

	int sizeY() const {
		return info().getInt("sizeY");
	}
};

template <>
class RuntimeInput<dv::Frame> : public RuntimeInputCommon<dv::Frame> {
public:
	RuntimeInput(const std::string &name, dvModuleData moduleData) : RuntimeInputCommon(name, moduleData) {
	}

	int sizeX() const {
		return info().getInt("sizeX");
	}

	int sizeY() const {
		return info().getInt("sizeY");
	}
};


class RuntimeInputs {
private:
	dvModuleData moduleData;

public:
	RuntimeInputs(dvModuleData m) : moduleData(m) {
	}

	/**
	 * Returns the information about the input with the specified name.
	 * The type of the input has to be specified as well.
	 * @tparam T The type of the input
	 * @param name The name of the input
	 * @return An object to access the information about the input
	 */
	template <typename T>
	const RuntimeInput<T> getInput(const std::string &name) const {
		return RuntimeInput<T>(name, moduleData);
	}

	/**
	 * (Convenience) Function to get an event input
	 * @param name the name of the event input stream
	 * @return An object to access information about the input stream
	 */
	const RuntimeInput<dv::EventPacket> getEventInput(const std::string &name) const {
		return getInput<dv::EventPacket>(name);
	}

	/**
	 * (Convenience) Function to get an frame input
	 * @param name the name of the frame input stream
	 * @return An object to access information about the input stream
	 */
	const RuntimeInput<dv::Frame> getFrameInput(const std::string &name) const {
		return getInput<dv::Frame>(name);
	}

	/**
	 * (Convenience) Function to get an IMU input
	 * @param name the name of the IMU input stream
	 * @return An object to access information about the input stream
	 */
	const RuntimeInput<dv::IMUPacket> getIMUInput(const std::string &name) const {
		return getInput<dv::IMUPacket>(name);
	}

	/**
	 * (Convenience) Function to get an trigger input
	 * @param name the name of the trigger input stream
	 * @return An object to access information about the input stream
	 */
	const RuntimeInput<dv::TriggerPacket> getTriggerInput(const std::string &name) const {
		return getInput<dv::TriggerPacket>(name);
	}


	/**
	 * Returns an info node about the specified input. Can be used to determine dimensions of an
	 * input/output
	 * @return A node that contains the specified inputs information, such as "sizeX" or "sizeY"
	 */
	const dv::Config::Node getUntypedInfo(const std::string &name) const {
		// const_cast and then re-add const manually. Needed for transition to C++ type.
		return (const_cast<dvConfigNode>(dvModuleInputGetInfoNode(moduleData, name.c_str())));
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

	template<typename T> OutputDataWrapper<T> get(const std::string &name) {
		OutputDataWrapper<T> wrapper{allocateUnwrapped<T>(name), moduleData, name};
		return (wrapper);
	}

	/**
	 * (Convenience) Function to get events from an event output
	 * @param name the name of the event output stream
	 * @return An output wrapper of an event packet, allowing data access
	 */
	OutputDataWrapper<dv::EventPacket> getEvents(const std::string &name) {
		return get<dv::EventPacket>(name);
	}

	/**
	 * (Convenience) Function to get a frame from a frame output
	 * @param name the name of the event output stream
	 * @return An output wrapper of a frame, allowing data access
	 */
	OutputDataWrapper<dv::Frame> getFrame(const std::string &name) {
		return get<dv::Frame>(name);
	}

	/**
	 * (Convenience) Function to get IMU data from an IMU output
	 * @param name the name of the event output stream
	 * @return An output wrapper of an IMU packet, allowing data access
	 */
	OutputDataWrapper<dv::IMUPacket> getIMUMeasurements(const std::string &name) {
		return get<dv::IMUPacket>(name);
	}

	/**
 	 * (Convenience) Function to get Trigger data from a Trigger output
 	 * @param name the name of the event output stream
 	 * @return An output wrapper of an Trigger packet, allowing data access
 	 */
	OutputDataWrapper<dv::TriggerPacket> getTriggers(const std::string &name) {
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
