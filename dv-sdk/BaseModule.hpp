#ifndef DV_SDK_BASE_MODULE_HPP
#define DV_SDK_BASE_MODULE_HPP

#include "config.hpp"
#include "log.hpp"
#include "module.h"
#include "utils.h"

#include <utility>

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
			typedObject->obj, [moduleData = moduleData, name, typedObject]() {
				dvModuleInputDismiss(moduleData, name.c_str(), typedObject);
			}};

#ifndef NDEBUG
		if (typedObject->typeId != *(reinterpret_cast<const uint32_t *>(T::identifier))) {
			throw std::runtime_error(
				"getUnwrapped(" + name + "): input type and given template type are not compatible.");
		}
#endif

		return (objPtr);
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
		if (typedObject->typeId != *(reinterpret_cast<const uint32_t *>(T::identifier))) {
			throw std::runtime_error(
				"allocateUnwrapped(" + name + "): output type and given template type are not compatible.");
		}
#endif

		return (typedObject->obj);
	}

	void commitUnwrapped(const std::string &name) {
		dvModuleOutputCommit(moduleData, name.c_str());
	}

	dv::Config::Node getInfoNode(const std::string &name) {
		return (dvModuleOutputGetInfoNode(moduleData, name.c_str()));
	}
};

/**
 * The dv BaseModule. Every module shall inherit from this module.
 * The base Module provides the following:
 * - Abstraction of the configuration
 * - Input / output management
 */
class BaseModule {
private:
	inline static thread_local dvModuleData __moduleData                  = nullptr;
	inline static std::function<void(RuntimeConfig &)> __getDefaultConfig = nullptr;

public:
	/**
	 * Static config init function. Calles the user provided `getConfigOptions` function
	 * which exists in this class as a static called `__getDefaultConfig`.
	 * It generates the default config and creates the elements for the default
	 * config in the DV config tree.
	 *
	 * @param moduleNode The dvConfig node for which the config should be generated.
	 */
	static void staticConfigInit(dv::Config::Node moduleNode) {
		// read config options from static user provided function
		RuntimeConfig defaultConfig{moduleNode};
		__getDefaultConfig(defaultConfig);
	}

	/**
	 * __INTERNAL USE ONLY__
	 * Sets the static, thread local module data to be used by
	 * a subsequent constructor. This shall only be used prior to
	 * @param _moduleData The moduleData param to be used for
	 * BaseModule member initialization upon constructor
	 */
	static void __setStaticModuleData(dvModuleData _moduleData) {
		__moduleData = _moduleData;
	}

	/**
	 * __INTERNAL USE ONLY__
	 * Sets the `__getDefaultConfig` static function to the user provided
	 * static function that generates the default config map.
	 * The reference to this function is used since there is no access
	 * to the child - subclass static functions possible from this class.
	 * The default config is both generated before instantiation in
	 * a call to `staticConfigInit` as well as in the constructor
	 * at runtime.
	 * @param _getDefaultConfig
	 */
	static void __setStaticGetDefaultConfig(std::function<void(RuntimeConfig &)> _getDefaultConfig) {
		__getDefaultConfig = std::move(_getDefaultConfig);
	}

	/**
	 * DV low-level module data. Use it to access the low-level DV API.
	 */
	dvModuleData moduleData;

	/**
	 * Loggers for the module. Each module has their own to avoid interference.
	 */
	Logger log;

	/**
	 * The module configuration node. Use it to access configuration at
	 * a lower level than the RuntimeConfigMap.
	 */
	dv::Config::Node moduleNode;

	/**
	 * Allows easy access to configuration data and is automatically
	 * updated with new values on changes in the configuration tree.
	 */
	RuntimeConfig config;

	/**
	 * Access data inputs and related actions in a type-safe manner.
	 */
	RuntimeInputs inputs;

	/**
	 * Access data outputs and related actions in a type-safe manner.
	 */
	RuntimeOutputs outputs;

	/**
	 * Base module constructor. The base module constructor initializes
	 * the logger and config members of the class, by utilizing the
	 * `static_thread` local pointer to the DV moduleData pointer
	 * provided prior to constructrion. This makes sure, that logger
	 * and config are available at the time the subclass constructor is
	 * called.
	 */
	BaseModule() :
		moduleData(__moduleData),
		moduleNode(moduleData->moduleNode),
		config(moduleNode),
		inputs(moduleData),
		outputs(moduleData) {
		assert(__moduleData);

		// Initialize the config map with the default config.
		__getDefaultConfig(config);

		// Add standard config.
		config.add("logLevel",
			dv::ConfigOption::intOption(moduleNode.getAttributeDescription<dv::Config::AttributeType::INT>("logLevel"),
				moduleNode.get<dv::Config::AttributeType::INT>("logLevel"), CAER_LOG_EMERGENCY, CAER_LOG_DEBUG));
	}

	virtual ~BaseModule() {
	}

	/**
	 * Method that updates the configs in the map as soon as some config
	 * change.
	 */
	void configUpdate() {
		for (auto &entry : config) {
			auto &key = entry.first;
			auto &cfg = entry.second;

			cfg.updateValue(key, moduleNode);
		}

		advancedConfigUpdate();
	}

	/**
	 * Virtual function to be implemented by the user. Can be left empty.
	 * Called on configuration update, allows more advanced control of how
	 * configuration values are updated.
	 */
	virtual void advancedConfigUpdate() {
	}

	/**
	 * Virtual function to be implemented by the user.
	 * Main function that runs the module and handles data.
	 */
	virtual void run() = 0;
};

} // namespace dv

#endif // DV_SDK_BASE_MODULE_HPP
