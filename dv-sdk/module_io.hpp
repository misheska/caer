#ifndef DV_SDK_MODULE_IO_HPP
#define DV_SDK_MODULE_IO_HPP

#include <dv-sdk/data/event.hpp>
#include <dv-sdk/data/frame.hpp>
#include <dv-sdk/data/imu.hpp>
#include <dv-sdk/data/trigger.hpp>
#include "data/wrappers.hpp"
#include "module.h"
#include "utils.h"

// Allow disabling of OpenCV requirement.
#ifndef DV_API_OPENCV_SUPPORT
#	define DV_API_OPENCV_SUPPORT 1
#endif

#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
#	include <opencv2/core.hpp>
#endif


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
class _RuntimeInputCommon {
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
	_RuntimeInputCommon(const std::string &name, dvModuleData moduleData) : name_(name), moduleData_(moduleData) {
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
	const dv::Config::Node infoNode() const {
		// const_cast and then re-add const manually. Needed for transition to C++ type.
		return (const_cast<dvConfigNode>(dvModuleInputGetInfoNode(moduleData_, name_.c_str())));
	}

	/**
	 * Returns true, if this optional input is actually connected to an output of another module
	 * @return true, if this input is connected
	 */
	bool isConnected() const {
		return (dvModuleInputIsConnected(moduleData_, name_.c_str()));
	}

	/**
	 * Returns the description of the origin of the data
	 * @return the description of the origin of the data
	 */
	const std::string& getOriginDescription() const {
	    return infoNode().getString(name_);
	}

};

template <typename T>
class RuntimeInput : public _RuntimeInputCommon<T> {
public:
	RuntimeInput(const std::string &name, dvModuleData moduleData) : _RuntimeInputCommon<T>(name, moduleData) {
	}
};

template <>
class RuntimeInput<dv::EventPacket> : public _RuntimeInputCommon<dv::EventPacket> {
public:
	RuntimeInput(const std::string &name, dvModuleData moduleData) : _RuntimeInputCommon(name, moduleData) {
	}

    InputDataWrapper<dv::EventPacket> events() const {
	    return data();
	}

	int sizeX() const {
		return infoNode().getInt("sizeX");
	}

	int sizeY() const {
		return infoNode().getInt("sizeY");
	}

#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
	const cv::Size size() const {
		return cv::Size(sizeX(), sizeY());
	}
#endif
};

template <>
class RuntimeInput<dv::Frame> : public _RuntimeInputCommon<dv::Frame> {
public:
	RuntimeInput(const std::string &name, dvModuleData moduleData) : _RuntimeInputCommon(name, moduleData) {
	}

    InputDataWrapper<dv::Frame> frame() const {
        return data();
    }

	int sizeX() const {
		return infoNode().getInt("sizeX");
	}

	int sizeY() const {
		return infoNode().getInt("sizeY");
	}

#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
	const cv::Size size() const {
		return cv::Size(sizeX(), sizeY());
	}
#endif
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

template <typename T>
class _RuntimeOutputCommon {
private:
	std::string name_;
	dvModuleData moduleData_;

	typename T::NativeTableType *allocateUnwrapped(const std::string &name) {
		auto typedObject = dvModuleOutputAllocate(moduleData_, name.c_str());
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
		dvModuleOutputCommit(moduleData_, name.c_str());
	}

public:
	_RuntimeOutputCommon(const std::string &name, dvModuleData moduleData) : name_(name),
	moduleData_(moduleData) {
	}

	OutputDataWrapper<T> getOutputData() {
		OutputDataWrapper<T> wrapper{allocateUnwrapped(name_), moduleData_, name_};
		return (wrapper);
	}

	dv::Config::Node infoNode() const {
		// const_cast and then re-add const manually. Needed for transition to C++ type.
		return (const_cast<dvConfigNode>(dvModuleInputGetInfoNode(moduleData_, name_.c_str())));
	}

};


template <typename T>
class RuntimeOutput : public _RuntimeOutputCommon<T> {
public:
	RuntimeOutput(const std::string &name, dvModuleData moduleData) : _RuntimeOutputCommon<T>(name, moduleData) {
	}

	void setup(const std::string &originDescription) {
		// dv::Config::Node infoNode = this->infoNode();
		auto infoNode = this->infoNode();
	    //infoNode.create<dv::Config::AttributeType::STRING>("source",
	    //		"", {0, 100}, dv::Cfg::AttributeFlags::NORMAL | dv::Cfg::AttributeFlags::NO_EXPORT, "");

		infoNode.create<dv::Config::AttributeType::INT>("myInt", 10, {0, 100}, dv::Cfg::AttributeFlags::NORMAL, "");

		std::printf("abc %d", 100);
	}
};

template <>
class RuntimeOutput<dv::EventPacket> : public _RuntimeOutputCommon<dv::EventPacket> {
public:
    RuntimeOutput(const std::string &name, dvModuleData moduleData) : _RuntimeOutputCommon<dv::EventPacket>(name, moduleData) {
    }

    void setup() {
		this->infoNode().create<dv::Config::AttributeType::INT>("myInt", 10, {0, 100}, dv::Cfg::AttributeFlags::NORMAL, "");
	}
};


class RuntimeOutputs {
private:
	dvModuleData moduleData_;

public:
	RuntimeOutputs(dvModuleData moduleData) : moduleData_(moduleData) {
	}

	template <typename T>
	RuntimeOutput<T> getOutput(const std::string &name) {
	    return RuntimeOutput<T>(name, moduleData_);
	}


	/**
	 * (Convenience) Function to get events from an event output
	 * @param name the name of the event output stream
	 * @return An output wrapper of an event packet, allowing data access
	 */
	RuntimeOutput<dv::EventPacket> getEventOutput(const std::string &name) {
		return getOutput<dv::EventPacket>(name);
	}

	/**
	 * (Convenience) Function to get a frame from a frame output
	 * @param name the name of the event output stream
	 * @return An output wrapper of a frame, allowing data access
	 */
	RuntimeOutput<dv::Frame> getFrameOutput(const std::string &name) {
		return getOutput<dv::Frame>(name);
	}

	/**
	 * (Convenience) Function to get IMU data from an IMU output
	 * @param name the name of the event output stream
	 * @return An output wrapper of an IMU packet, allowing data access
	 */
	RuntimeOutput<dv::IMUPacket> getIMUOutput(const std::string &name) {
		return getOutput<dv::IMUPacket>(name);
	}

	/**
 	 * (Convenience) Function to get Trigger data from a Trigger output
 	 * @param name the name of the event output stream
 	 * @return An output wrapper of an Trigger packet, allowing data access
 	 */
	RuntimeOutput<dv::TriggerPacket> getTriggerOutput(const std::string &name) {
		return getOutput<dv::TriggerPacket>(name);
	}

    /**
     * Returns an info node about the specified input. Can be used to determine dimensions of an
     * input/output
     * @return A node that contains the specified inputs information, such as "sizeX" or "sizeY"
     */
    const dv::Config::Node getUntypedInfo(const std::string &name) const {
        // const_cast and then re-add const manually. Needed for transition to C++ type.
        return (const_cast<dvConfigNode>(dvModuleInputGetInfoNode(moduleData_, name.c_str())));
    }
};

} // namespace dv

#endif // DV_SDK_MODULE_IO_HPP
