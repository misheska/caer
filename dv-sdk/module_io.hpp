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


/**
 * Vector decorator that gives convenience methods to add various kinds of inputs to
 * a module
 */
class InputDefinitionList  {
private:
	std::vector<InputDefinition> inputs;
public:
	/**
	 * Adds an input of a generic type to the module
	 * @param name The name of the input
	 * @param typeIdentifier The identifier of the flatbuffers data type used on this input
	 * @param optional A flag that describes if this input is optional or not.
	 */
	void addInput(const std::string &name, const std::string typeIdentifier, bool optional = false) {
		inputs.emplace_back(InputDefinition(name, typeIdentifier, optional));
	}

	/**
	 * Adds an input for event data to this module.
	 * @param name The name of this event data input
	 * @param optional A flag to set this input as optional
	 */
	void addEventInput(const std::string &name, bool optional = false) {
		addInput(name, dv::EventPacket::identifier, optional);
	}

	/**
	 * Adds a frame input to this module
	 * @param name The name of this input
	 * @param optional A flag to set this input as optional
	 */
	void addFrameInput(const std::string &name, bool optional = false) {
		addInput(name, dv::Frame::identifier, optional);
	}

	/**
	 * Adds an IMU input to this module
	 * @param name The name of this input
	 * @param optional A flag to set this input as optional
	 */
	void addIMUInput(const std::string &name, bool optional = false) {
		addInput(name, dv::IMUPacket::identifier, optional);
	}

	/**
	 * Adds a trigger input to this module.
	 * @param name The name of the trigger module
	 * @param optional A flag to set this input as optional
	 */
	void addTriggerInput(const std::string &name, bool optional = false) {
		addInput(name, dv::TriggerPacket::identifier, optional);
	}

	/**
	 * __INTERNAL USE__
	 * Returns the list of configured input definitions
	 * @return The list of configured input definitions
	 */
	const std::vector<InputDefinition> &getInputs() const {
		return inputs;
	}
};


/**
 * Vector decorator that exposes convenience functions to add various types of outputs to
 * a module
 */
class OutputDefinitionList {
private:
	std::vector<OutputDefinition> outputs;
public:
	/**
	 * Adds an output to the module of a generic type
	 * @param name The name of this output
	 * @param typeIdentifier The flatbuffers type identifier for this type
	 */
	void addOutput(const std::string &name, const std::string &typeIdentifier) {
		outputs.emplace_back(OutputDefinition(name, typeIdentifier));
	}

	/**
	 * Adds an event output to this module
	 * @param name The name of this output
	 */
	void addEventOutput(const std::string &name) {
		addOutput(name, dv::EventPacket::identifier);
	}

	/**
	 * Adds a frame output to this module
	 * @param name The name of this output
	 */
	void addFrameOutput(const std::string &name) {
		addOutput(name, dv::Frame::identifier);
	}

	/**
	 * Adds an IMU output to this module
	 * @param name The name of this output
	 */
	void addIMUOutput(const std::string &name) {
		addOutput(name, dv::IMUPacket::identifier);
	}

	/**
	 * Adds a trigger output to this module
	 * @param name the name of this output
	 */
	void addTriggerOutput(const std::string &name) {
		addOutput(name, dv::TriggerPacket::identifier);
	}

	/**
	 * __INTERNAL USE__
	 * Returns the list of configured outputs
	 * @return the list of configured outputs
	 */
	const std::vector<OutputDefinition> &getOutputs() const {
		return outputs;
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
	    return infoNode().getString("source");
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
		return (const_cast<dvConfigNode>(dvModuleOutputGetInfoNode(moduleData_, name_.c_str())));
	}

protected:
    void createSourceAttribute(const std::string &originDescription) {
        dv::Config::Node infoNode = this->infoNode();
        infoNode.create<dv::Config::AttributeType::STRING>("source",
                originDescription, {0, PATH_MAX}, dv::Cfg::AttributeFlags::NORMAL | dv::Cfg::AttributeFlags::NO_EXPORT,
                "Description of the first origin of the data");
    }

    void createSizeAttributes(int sizeX, int sizeY) {
        dv::Config::Node infoNode = this->infoNode();
        infoNode.create<dv::Config::AttributeType::INT>("sizeX",
                sizeX, {sizeX, sizeX}, dv::Cfg::AttributeFlags::NORMAL | dv::Cfg::AttributeFlags::NO_EXPORT,
                "Width of the output data. (max x-coordinate + 1)");

        infoNode.create<dv::Config::AttributeType::INT>("sizeY",
                sizeY, {sizeY, sizeY}, dv::Cfg::AttributeFlags::NORMAL | dv::Cfg::AttributeFlags::NO_EXPORT,
                "Height of the output data. (max y-coordinate + 1)");
    }
};


template <typename T>
class RuntimeOutput : public _RuntimeOutputCommon<T> {
public:
	RuntimeOutput(const std::string &name, dvModuleData moduleData) : _RuntimeOutputCommon<T>(name, moduleData) {
	}

	void setup(const std::string &originDescription) {
        this->createSourceAttribute(originDescription);
	}
};

template <>
class RuntimeOutput<dv::EventPacket> : public _RuntimeOutputCommon<dv::EventPacket> {
public:
    RuntimeOutput(const std::string &name, dvModuleData moduleData) : _RuntimeOutputCommon<dv::EventPacket>(name, moduleData) {
    }

    void setup(int sizeX, int sizeY, const std::string &originDescription) {
        this->createSourceAttribute(originDescription);
        this->createSizeAttributes(sizeX, sizeY);
	}

    void setup(const RuntimeInput<dv::EventPacket> &eventInput) {
        setup(eventInput.sizeX(), eventInput.sizeY(), eventInput.getOriginDescription());
    }

    void setup(const RuntimeInput<dv::Frame> &frameInput) {
        setup(frameInput.sizeX(), frameInput.sizeY(), frameInput.getOriginDescription());
    }
};

template <>
class RuntimeOutput<dv::Frame> : public _RuntimeOutputCommon<dv::Frame> {
public:
    RuntimeOutput(const std::string &name, dvModuleData moduleData) : _RuntimeOutputCommon<dv::Frame>(name, moduleData) {
    }

    void setup(int sizeX, int sizeY, const std::string &originDescription) {
        this->createSourceAttribute(originDescription);
        this->createSizeAttributes(sizeX, sizeY);
    }

    void setup(const RuntimeInput<dv::EventPacket> &eventInput) {
        setup(eventInput.sizeX(), eventInput.sizeY(), eventInput.getOriginDescription());
    }

    void setup(const RuntimeInput<dv::Frame> &frameInput) {
        setup(frameInput.sizeX(), frameInput.sizeY(), frameInput.getOriginDescription());
    }


#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
	RuntimeOutput<dv::Frame>& operator<<(const cv::Mat &mat) {
		getOutputData() << mat;
		return *this;
	}
#endif

};



class RuntimeOutputs {
private:
	dvModuleData moduleData_;

public:
	RuntimeOutputs(dvModuleData moduleData) : moduleData_(moduleData) {
	}

    /**
     * Function to get an output
     * @param name the name of the output stream
     * @return An object to access the modules output
     */
	template <typename T>
	RuntimeOutput<T> getOutput(const std::string &name) {
	    return RuntimeOutput<T>(name, moduleData_);
	}


	/**
	 * (Convenience) Function to get an event output
	 * @param name the name of the event output stream
	 * @return An object to access the modules output
	 */
	RuntimeOutput<dv::EventPacket> getEventOutput(const std::string &name) {
		return getOutput<dv::EventPacket>(name);
	}

    /**
     * (Convenience) Function to get an frame output
     * @param name the name of the frame output stream
     * @return An object to access the modules output
     */
	RuntimeOutput<dv::Frame> getFrameOutput(const std::string &name) {
		return getOutput<dv::Frame>(name);
	}

    /**
     * (Convenience) Function to get an imu output
     * @param name the name of the imu output stream
     * @return An object to access the modules output
     */
	RuntimeOutput<dv::IMUPacket> getIMUOutput(const std::string &name) {
		return getOutput<dv::IMUPacket>(name);
	}

    /**
     * (Convenience) Function to get an trigger output
     * @param name the name of the trigger output stream
     * @return An object to access the modules output
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
