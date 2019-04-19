#ifndef DV_SDK_MODULE_BASE_HPP
#define DV_SDK_MODULE_BASE_HPP

#include "config.hpp"
#include "log.hpp"
#include "module.h"
#include "module_io.hpp"
#include "utils.h"

#include <utility>

namespace dv {

/**
 * The dv ModuleBase. Every module shall inherit from this module.
 * The base Module provides the following:
 * - Abstraction of the configuration
 * - Input / output management
 */
class ModuleBase {
private:
	inline static thread_local dvModuleData __moduleData                               = nullptr;
	inline static thread_local std::function<void(RuntimeConfig &)> __getDefaultConfig = nullptr;

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
	 * ModuleBase member initialization upon constructor
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
	ModuleBase() :
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
				CAER_LOG_NOTICE, CAER_LOG_EMERGENCY, CAER_LOG_DEBUG));
		config.add(
			"running", dv::ConfigOption::boolOption(
						   moduleNode.getAttributeDescription<dv::Config::AttributeType::BOOL>("running"), true));
	}

	virtual ~ModuleBase() {
	}

	/**
	 * Method that updates the configs in the map as soon as some config
	 * change.
	 */
	void configUpdate() {
		config.update();

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

#endif // DV_SDK_MODULE_BASE_HPP
