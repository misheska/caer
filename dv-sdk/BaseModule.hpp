#ifndef DV_SDK_BASE_MODULE_HPP
#define DV_SDK_BASE_MODULE_HPP

#include "config.hpp"
#include "log.hpp"
#include "module.h"

#include <unordered_map>
#include <utility>

namespace dv {

/**
 * The dv BaseModule. Every module shall inherit from this module.
 * The base Module provides the following:
 * - Abstraction of the configuration
 * - Input / output management
 */
class BaseModule {
private:
	inline static thread_local dvModuleData __moduleData                                                  = nullptr;
	inline static std::function<void(std::unordered_map<std::string, ConfigOption> &)> __getDefaultConfig = nullptr;

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
		std::unordered_map<std::string, ConfigOption> defaultConfig;
		__getDefaultConfig(defaultConfig);

		for (auto &entry : defaultConfig) {
			auto &key = entry.first;
			auto &cfg = entry.second;

			cfg.createAttribute(key, moduleNode);
		}
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
	static void __setStaticGetDefaultConfig(
		std::function<void(std::unordered_map<std::string, ConfigOption> &)> _getDefaultConfig) {
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
	 * Map that allows easy access to configuration data and is automatically
	 * updated with new values on changes from outside.
	 */
	std::unordered_map<std::string, ConfigOption> config;

	/**
	 * Base module constructor. The base module constructor initializes
	 * the logger and config members of the class, by utilizing the
	 * `static_thread` local pointer to the DV moduleData pointer
	 * provided prior to constructrion. This makes sure, that logger
	 * and config are available at the time the subclass constructor is
	 * called.
	 */
	BaseModule() : moduleData(__moduleData), moduleNode(__moduleData->moduleNode) {
		assert(__moduleData);

		// Initialize the config map with the default config.
		__getDefaultConfig(config);

		// Add standard config.
		config["logLevel"] = dv::ConfigOption::intOption(
			moduleNode.getAttributeDescription<dv::Config::AttributeType::INT>("logLevel"),
			moduleNode.get<dv::Config::AttributeType::INT>("logLevel"), CAER_LOG_EMERGENCY, CAER_LOG_DEBUG);

		// Update the config values with the latest changes.
		configUpdate();
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

	dv::Config::Node outputGetInfoNode(const std::string &name) {
		return (dvModuleOutputGetInfoNode(moduleData, name.c_str()));
	}

	const dv::Config::Node inputGetInfoNode(const std::string &name) {
		return (const_cast<dvConfigNode>(dvModuleInputGetInfoNode(moduleData, name.c_str())));
	}

	const dv::Config::Node inputGetUpstreamNode(const std::string &name) {
		return (const_cast<dvConfigNode>(dvModuleInputGetUpstreamNode(moduleData, name.c_str())));
	}
};

} // namespace dv

#endif // DV_SDK_BASE_MODULE_HPP
