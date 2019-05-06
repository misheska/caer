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

/**
 * Definition of a module input. Every input to a module has a unique (for the module) name,
 * as well as a type. The type is the 4-character flatbuffers identifier
 */
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

/**
 * Definition of a module output. Every output has a unique (for the module) name,
 * as well as a type. THe type is the 4-character flatbuffers identifier.
 */
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
class InputDefinitionList {
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

/**
 * Base class for a runtime input definition.
 * There are template-specialized subclasses of this, providing convenience function
 * interfaces for the most common, known types. There is also a generic,
 * templated subclass `RuntimeInput` which does not add any more convenience
 * functions over this common subclass, and can be used for the generic case
 */
template<typename T> class _RuntimeInputCommon {
private:
	/* Runtime name of this module from config */
	std::string name_;
	/* Pointer to the dv moduleData struct */
	dvModuleData moduleData_;

	/**
	 * Fetches available data at the input and returns a shared_ptr to it.
	 * Also casts the shared_ptr to this particular input type.
	 * @return A shared_ptr of the input data type to the latest received data
	 */
	std::shared_ptr<const typename T::NativeTableType> getUnwrapped() const {
		auto typedObject = dvModuleInputGet(moduleData_, name_.c_str());
		if (typedObject == nullptr) {
			// Actual errors will write a log message and return null.
			// No data just returns null. So if null we simply forward that.
			return (nullptr);
		}

		// Build shared_ptr with custom deleter first, so that in verification failure case
		// (debug mode), memory gets properly cleaned up.
		std::shared_ptr<const typename T::NativeTableType> objPtr{
			static_cast<const typename T::NativeTableType *>(typedObject->obj),
			[moduleData = moduleData_, name = name_, typedObject](
				const typename T::NativeTableType *) { dvModuleInputDismiss(moduleData, name.c_str(), typedObject); }};

#ifndef NDEBUG
		if (typedObject->typeId != dvTypeIdentifierToId(T::identifier)) {
			throw std::runtime_error(
				"getUnwrapped(" + name_ + "): input type and given template type are not compatible.");
		}
#endif

		return (objPtr);
	}

public:
	/**
	 * This constructor is called by the child classes in their initialization
	 * @param name The name of this input
	 * @param moduleData Pointer to the dv moduleData struct
	 */
	_RuntimeInputCommon(const std::string &name, dvModuleData moduleData) : name_(name), moduleData_(moduleData) {
	}

	/**
	 * Get data from an input
	 * @param name The name of the input stream
	 * @return An input wrapper of the desired type, allowing data access
	 */
	const InputDataWrapper<T> data() const {
		const InputDataWrapper<T> wrapper{getUnwrapped()};
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
	const std::string getOriginDescription() const {
		return infoNode().getString("source");
	}
};

/**
 * Describes a generic input at runtime. A generic input can be instantiated for any type.
 * This class basically just inherits from `_RuntimeInputCommon<T>` and does not add any
 * specializations.
 * @tparam T The type of the input data
 */
template<typename T> class RuntimeInput : public _RuntimeInputCommon<T> {
public:
	RuntimeInput(const std::string &name, dvModuleData moduleData) : _RuntimeInputCommon<T>(name, moduleData) {
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
	InputDataWrapper<dv::EventPacket> events() const {
		return data();
	}

	/**
	 * @return The width of the input region in pixels. Any event on this input will have a x-coordinate
	 * smaller than the return value of this function.
	 */
	int sizeX() const {
		return infoNode().getInt("sizeX");
	}

	/**
	 * @return The height of the input region in pixels. Any event on this input will have a y-coordinate
	 * smaller than the return value of this function
	 */
	int sizeY() const {
		return infoNode().getInt("sizeY");
	}

#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
	/**
	 * @return the input region size in pixels as an OpenCV size object
	 */
	const cv::Size size() const {
		return cv::Size(sizeX(), sizeY());
	}
#endif
};

template<> class RuntimeInput<dv::Frame> : public _RuntimeInputCommon<dv::Frame> {
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
	template<typename T> const RuntimeInput<T> getInput(const std::string &name) const {
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
	const dv::Config::Node infoNode(const std::string &name) const {
		// const_cast and then re-add const manually. Needed for transition to C++ type.
		return (const_cast<dvConfigNode>(dvModuleInputGetInfoNode(moduleData, name.c_str())));
	}
};

/**
 * Base class for a runtime output. This class acts as the base for various template-specialized
 * sub classes which provide convenience functions for outputting data in their respective
 * data types. There is a templated generic subclass `RuntimeOutput<T>` that can be used
 * for the generic case
 * @tparam T The flatbuffers type of the output data
 */
template<typename T> class _RuntimeOutputCommon {
private:
	/* Configured name of the module at runtime */
	std::string name_;
	/* pointer to the dv moduleData struct at runtime */
	dvModuleData moduleData_;

	/**
	 * Allocates a new instance of the datatype of this output and returns a
	 * raw pointer to the allocated memory. If there was memory allocated before
	 * (This function has been called before) but the output never has been commited,
	 * a raw pointer to the previously allocated memory gets returned.
	 * @return A raw pointer to the allocated memory
	 */
	typename T::NativeTableType *allocateUnwrapped() {
		auto typedObject = dvModuleOutputAllocate(moduleData_, name_.c_str());
		if (typedObject == nullptr) {
			// Actual errors will write a log message and return null.
			// No data just returns null. So if null we simply forward that.
			return (nullptr);
		}

#ifndef NDEBUG
		if (typedObject->typeId != dvTypeIdentifierToId(T::identifier)) {
			throw std::runtime_error(
				"allocateUnwrapped(" + name_ + "): output type and given template type are not compatible.");
		}
#endif

		return (static_cast<typename T::NativeTableType *>(typedObject->obj));
	}

public:
	/**
	 * This constructor is called by the subclasses constructors
	 * @param name The configuration name of the module this output belongs to
	 * @param moduleData A pointer to the dv moduleData struct
	 */
	_RuntimeOutputCommon(const std::string &name, dvModuleData moduleData) : name_(name), moduleData_(moduleData) {
	}

	/**
	 * Returns a writeable output wrapper for the given type of this output.
	 * Allocates new output memory if necessary. The output can be committed
	 * by calling commit on the returned object.
	 * @return A wrapper to allocated output memory to write to
	 */
	OutputDataWrapper<T> getOutputData() {
		OutputDataWrapper<T> wrapper{allocateUnwrapped(), moduleData_, name_};
		return (wrapper);
	}

	/**
	 * Returns the node of the dv config tree that contains all the information
	 * about this output.
	 * @return The node in the dv config tree with the information about this output
	 */
	dv::Config::Node infoNode() const {
		// const_cast and then re-add const manually. Needed for transition to C++ type.
		return (const_cast<dvConfigNode>(dvModuleOutputGetInfoNode(moduleData_, name_.c_str())));
	}

protected:
	/**
	 * Creates the output information attribute in the config tree.
	 * The source attribute is a string containing information about the original generator of the data
	 * @param originDescription a string containing information about the original generator of the data
	 */
	void createSourceAttribute(const std::string &originDescription) {
		dv::Config::Node infoNode = this->infoNode();
		infoNode.create<dv::Config::AttributeType::STRING>("source", originDescription, {0, PATH_MAX},
			dv::Cfg::AttributeFlags::NORMAL | dv::Cfg::AttributeFlags::NO_EXPORT,
			"Description of the first origin of the data");
	}

	/**
	 * Adds size information attributes to the output info node
	 * @param sizeX The width dimension of the output
	 * @param sizeY The height dimension of the output
	 */
	void createSizeAttributes(int sizeX, int sizeY) {
		dv::Config::Node infoNode = this->infoNode();
		infoNode.create<dv::Config::AttributeType::INT>("sizeX", sizeX, {sizeX, sizeX},
			dv::Cfg::AttributeFlags::NORMAL | dv::Cfg::AttributeFlags::NO_EXPORT,
			"Width of the output data. (max x-coordinate + 1)");

		infoNode.create<dv::Config::AttributeType::INT>("sizeY", sizeY, {sizeY, sizeY},
			dv::Cfg::AttributeFlags::NORMAL | dv::Cfg::AttributeFlags::NO_EXPORT,
			"Height of the output data. (max y-coordinate + 1)");
	}
};

/**
 * Class that describes an output of a generic type at runtime.
 * Can be used to obtain information about the output, as well as getting a new
 * output object to send data to.
 * @tparam T The flatbuffers type of the output
 */
template<typename T> class RuntimeOutput : public _RuntimeOutputCommon<T> {
public:
	RuntimeOutput(const std::string &name, dvModuleData moduleData) : _RuntimeOutputCommon<T>(name, moduleData) {
	}

	/**
	 * Sets up the output. Has to be called in the constructor of the module.
	 * @param originDescription A description of the original creator of the data
	 */
	void setup(const std::string &originDescription) {
		this->createSourceAttribute(originDescription);
	}
};

/**
 * Specialization of the runtime output for event outputs.
 * Provides convenience setup functions for setting up the event output
 */
template<> class RuntimeOutput<dv::EventPacket> : public _RuntimeOutputCommon<dv::EventPacket> {
public:
	RuntimeOutput(const std::string &name, dvModuleData moduleData) :
		_RuntimeOutputCommon<dv::EventPacket>(name, moduleData) {
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
	void setup(const RuntimeInput<dv::Frame> &frameInput) {
		setup(frameInput.sizeX(), frameInput.sizeY(), frameInput.getOriginDescription());
	}
};

/**
 * Specialization of the runtime output for frame outputs
 * Provides convenience setup functions for setting up the frame output
 */
template<> class RuntimeOutput<dv::Frame> : public _RuntimeOutputCommon<dv::Frame> {
public:
	RuntimeOutput(const std::string &name, dvModuleData moduleData) :
		_RuntimeOutputCommon<dv::Frame>(name, moduleData) {
	}

	/**
	 * Sets up this frame output with the provided parameters
	 * @param sizeX The width of the frames supplied on this output
	 * @param sizeY The height of the frames supplied on this output
	 * @param originDescription A description of the original creator of the data
	 */
	void setup(int sizeX, int sizeY, const std::string &originDescription) {
		this->createSourceAttribute(originDescription);
		this->createSizeAttributes(sizeX, sizeY);
	}

	/**
	 * Sets this frame output up with the same parameters as the supplied input.
	 * @param eventInput The event input to copy the information from
	 */
	void setup(const RuntimeInput<dv::EventPacket> &eventInput) {
		setup(eventInput.sizeX(), eventInput.sizeY(), eventInput.getOriginDescription());
	}

	/**
	 * Sets this frame output up with the same parameters the the supplied input.
	 * @param frameInput  The frame input to copy the data from
	 */
	void setup(const RuntimeInput<dv::Frame> &frameInput) {
		setup(frameInput.sizeX(), frameInput.sizeY(), frameInput.getOriginDescription());
	}

#if defined(DV_API_OPENCV_SUPPORT) && DV_API_OPENCV_SUPPORT == 1
	/**
	 * Convenience shorthand to commit an OpenCV mat onto this output.
	 * If not using this function, call `getOutputData` to get an output frame
	 * to fill into.
	 * @param mat The OpenCV Mat to submit
	 * @return A reference to the this
	 */
	RuntimeOutput<dv::Frame> &operator<<(const cv::Mat &mat) {
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
	template<typename T> RuntimeOutput<T> getOutput(const std::string &name) {
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
