#ifndef DV_SDK_MODULE_IO_HPP
#define DV_SDK_MODULE_IO_HPP

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

	template<typename T> const InputWrapper<T> get(const std::string &name) const {
		const InputWrapper<T> wrapper{getUnwrapped<T>(name)};
		return (wrapper);
	}

	const dv::Config::Node getInfoNode(const std::string &name) const {
		// const_cast and then re-add const manually. Needed for transition to C++ type.
		return (const_cast<dvConfigNode>(dvModuleInputGetInfoNode(moduleData, name.c_str())));
	}

	const dv::Config::Node getUpstreamNode(const std::string &name) const {
		// const_cast and then re-add const manually. Needed for transition to C++ type.
		return (const_cast<dvConfigNode>(dvModuleInputGetUpstreamNode(moduleData, name.c_str())));
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

	dv::Config::Node getInfoNode(const std::string &name) {
		return (dvModuleOutputGetInfoNode(moduleData, name.c_str()));
	}
};

} // namespace dv

#endif // DV_SDK_MODULE_IO_HPP
