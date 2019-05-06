#ifndef DV_SDK_WRAPPERS_HPP
#define DV_SDK_WRAPPERS_HPP

#include "../module.h"
#include "../utils.h"
#include "cvector_proxy.hpp"

namespace dv {

struct commitType {};
constexpr commitType commit{};

template<typename T> class InputDataWrapper {
private:
	using NativeType = typename T::NativeTableType;

	std::shared_ptr<const NativeType> ptr;

public:
	InputDataWrapper(std::shared_ptr<const NativeType> p) : ptr(std::move(p)) {
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

template<typename T> class OutputDataWrapper {
private:
	using NativeType = typename T::NativeTableType;

	NativeType *ptr;
	dvModuleData moduleData;
	std::string name;

public:
	OutputDataWrapper(NativeType *p, dvModuleData m, const std::string &n) : ptr(p), moduleData(m), name(n) {
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

	const OutputDataWrapper operator<<(commitType) {
		commit();
		return *this;
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
		return (infoNode().getString("source"));
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
	OutputDataWrapper<T> data() {
		OutputDataWrapper<T> wrapper{allocateUnwrapped(), moduleData_, name_};
		return (wrapper);
	}

	/**
	 * Returns an info node about the specified output, can be used toset output information.
	 * @return A node that can contain output information, such as "sizeX" or "sizeY"
	 */
	dv::Config::Node infoNode() {
		return (dvModuleOutputGetInfoNode(moduleData_, name_.c_str()));
	}

protected:
	/**
	 * Creates the output information attribute in the config tree.
	 * The source attribute is a string containing information about the original generator of the data
	 * @param originDescription a string containing information about the original generator of the data
	 */
	void createSourceAttribute(const std::string &originDescription) {
		dv::Config::Node infoNode = this->infoNode();
		infoNode.create<dv::Config::AttributeType::STRING>("source", originDescription, {0, 8192},
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

} // namespace dv

#endif // DV_SDK_WRAPPERS_HPP
