//
// Created by najiji on 15.01.19.
//

//
// Created by najiji on 11.01.19.
//

#ifndef CAER_MODULES_SDK_BASEMODULE_H
#define CAER_MODULES_SDK_BASEMODULE_H

#include <caer-sdk/module.h>

#include "log.hpp"

#include <boost/any.hpp>
#include <map>

/**
 * Returns the sign of the given number as -1 or 1. Returns 1 for 0.
 * @tparam T The data type of the number and return value
 * @param x the data to be checked
 * @return -1 iff x < 0, 1 otherwise
 */
template<class T> inline T sgn(T x) {
	return (x < (T) 0) ? (T) -1 : (T) 1;
}

namespace caer {

/**
 * Different opening modes for a File Dialog config option
 */
enum class _FileDialogMode { NONE, OPEN, SAVE, DIRECTORY };

/**
 * The different config variations a user can choose from, when building the
 * configuration of a module
 */
enum class _ConfigVariant { NONE, BOOLEAN, FILE, STRING, INTEGER, FRACTIONAL };

/**
 * Maps the selected config variant template to a C++ data type.
 */
template<_ConfigVariant> struct _ConfigVariantType {};
template<> struct _ConfigVariantType<_ConfigVariant::BOOLEAN> { typedef bool type; };
template<> struct _ConfigVariantType<_ConfigVariant::FILE> { typedef std::string type; };
template<> struct _ConfigVariantType<_ConfigVariant::STRING> { typedef std::string type; };
template<> struct _ConfigVariantType<_ConfigVariant::INTEGER> { typedef int64_t type; };
template<> struct _ConfigVariantType<_ConfigVariant::FRACTIONAL> { typedef double type; };

/**
 * Maps the selected config variant to a struct with additional config params
 * that have to be set for the selected config variant. Defaults to an empty struct.
 */
template<_ConfigVariant> struct _ConfigAttributes {};
template<> struct _ConfigAttributes<_ConfigVariant::FILE> {
	std::string allowedExtensions;
	_FileDialogMode mode = _FileDialogMode::NONE;
};
template<> struct _ConfigAttributes<_ConfigVariant::INTEGER> {
	using _VariantType = typename _ConfigVariantType<_ConfigVariant::INTEGER>::type;
	_VariantType min;
	_VariantType max;
};
template<> struct _ConfigAttributes<_ConfigVariant::FRACTIONAL> {
	using _VariantType = typename _ConfigVariantType<_ConfigVariant::FRACTIONAL>::type;
	_VariantType min;
	_VariantType max;
};

/**
 * Templated implementation class of a ConfigOption. Stores in a layout according
 * to the selected config variant.
 * @tparam V
 */
template<_ConfigVariant V> class _ConfigOption {
	using _VariantType = typename _ConfigVariantType<V>::type;

public:
	const std::string description;
	const _VariantType initValue;
	const _ConfigAttributes<V> attributes;
	_ConfigOption(const std::string &description_, _VariantType initValue_, const _ConfigAttributes<V> &attributes_) :
		description(description_),
		initValue(initValue_),
		attributes(attributes_) {
	}
};

/**
 * Non template class for a config option. Has a type independent shared pointer
 * that points to the actual (templated) config object.
 * Private constructor. Should only be used with static factory function.
 */
class ConfigOption {
private:
	std::shared_ptr<void> configOption_;
	_ConfigVariant variant_ = _ConfigVariant::NONE;

	/**
	 * __Private constructor__
	 * Takes shared_ptr to templated `_ConfigOption` as well as the variant.
	 * @param configOption A shared_ptr to an instantiated `_ConfigOption`
	 * @param variant The config variant of the passed option
	 */
	ConfigOption(std::shared_ptr<void> configOption, _ConfigVariant variant) :
		configOption_(std::move(configOption)),
		variant_(variant){};

	/**
	 * Private base factory method.
	 * Used as a base for all the config factories defined
	 * Creates a new ConfigOption of the requested type. Works by first instantiating
	 * a `_ConfigOption` with the right templated variant, then creating a `shared_ptr`
	 * and creating a `ConfigOption` to return.
	 * @tparam V
	 * @param description
	 * @param defaultValue
	 * @param attributes_
	 * @return
	 */
	template<_ConfigVariant V>
	static ConfigOption getOption(const std::string &description, typename _ConfigVariantType<V>::type defaultValue,
		const _ConfigAttributes<V> &attributes_) {
		return ConfigOption(
			std::shared_ptr<_ConfigOption<V>>(new _ConfigOption<V>(description, defaultValue, attributes_)), V);
	}

public:
	ConfigOption() = default;

	/**
	 * Returns the variant of this `ConfigOption`. The return value of this
	 * function can be used to use as a
	 * @return
	 */
	inline _ConfigVariant getVariant() const {
		return variant_;
	}

	/**
	 * Returns the underlying config object, casted to the specified configVariant.
	 * The config variant can be read out with the `getVariant` method.
	 * @tparam V The config variant to be casted to.
	 * @return The underlying _ConfigObject with the configuration data
	 */
	template<_ConfigVariant V> _ConfigOption<V> &getConfigObject() {
		return *(std::static_pointer_cast<_ConfigOption<V>>(configOption_));
	}

	// Static convenience factory methods -------------------------------------------------------
	/**
	 * Factory function. Creates a fractional config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @param minValue The min value a user can choose for this option
	 * @param maxValue The max value a user can choose for this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption fractionalOption(
		const std::string &description, double defaultValue, double minValue, double maxValue) {
		return getOption<_ConfigVariant::FRACTIONAL>(description, defaultValue, {minValue, maxValue});
	};

	/**
	 * Factory function. Creates a fractional config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @return A ConfigOption Object
	 */
	static ConfigOption fractionalOption(const std::string &description, double defaultValue) {
		double sensibleUpperRange
			= ((std::abs(defaultValue) > 0.) ? std::pow(10., std::floor(std::log10(std::abs(defaultValue)) + 1.)) : 1.)
			  * sgn(defaultValue);
		return getOption<_ConfigVariant::FRACTIONAL>(description, defaultValue, {0., sensibleUpperRange});
	};

	/**
	 * Factory function. Creates a integer config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @param minValue The min value a user can choose for this option
	 * @param maxValue The max value a user can choose for this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption integerOption(const std::string &description, long defaultValue, long minValue, long maxValue) {
		return getOption<_ConfigVariant::INTEGER>(description, defaultValue, {minValue, maxValue});
	}

	/**
	 * Factory function. Creates a integer config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @return A ConfigOption Object
	 */
	static ConfigOption integerOption(const std::string &description, long defaultValue) {
		long sensibleUpperRange
			= ((std::abs(defaultValue) > 0)
					  ? (long) std::pow(10., std::floor(std::log10((double) std::abs(defaultValue)) + 1.))
					  : 1)
			  * sgn(defaultValue);
		return getOption<_ConfigVariant::INTEGER>(description, defaultValue, {0, sensibleUpperRange});
	}

	/**
	 * Factory function. Creates a string config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @return A ConfigOption Object
	 */
	static ConfigOption stringOption(const std::string &description, const std::string &defaultValue) {
		return getOption<_ConfigVariant::STRING>(description, defaultValue, {});
	}

	/**
	 * Factory function. Creates a file open config option.
	 * @param description A description that describes the purpose of this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileOpenOption(const std::string &description) {
		return getOption<_ConfigVariant::FILE>(description, "", {".*", _FileDialogMode::OPEN});
	}

	/**
	 * Factory function. Creates a file open config option.
	 * @param description A description that describes the purpose of this option
	 * @param allowedExtensions The allowed extensions of the file to be opened
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileOpenOption(const std::string &description, const std::string &allowedExtensions) {
		return getOption<_ConfigVariant::FILE>(description, "", {allowedExtensions, _FileDialogMode::OPEN});
	}

	/**
	 * Factory function. Creates a file open config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default path of the file
	 * @param allowedExtensions The allowed extensions of the file to be opened
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileOpenOption(
		const std::string &description, const std::string &defaultValue, const std::string &allowedExtensions) {
		return getOption<_ConfigVariant::FILE>(description, defaultValue, {allowedExtensions, _FileDialogMode::OPEN});
	}

	/**
	 * Factory function. Creates a file save config option.
	 * @param description A description that describes the purpose of this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileSaveOption(const std::string &description) {
		return getOption<_ConfigVariant::FILE>(description, "", {"*", _FileDialogMode::SAVE});
	}

	/**
	 * Factory function. Creates a file save config option.
	 * @param description A description that describes the purpose of this option
	 * @param allowedExtensions The allowed extensions of the file to be saved
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileSaveOption(const std::string &description, const std::string &allowedExtensions) {
		return getOption<_ConfigVariant::FILE>(description, "", {allowedExtensions, _FileDialogMode::SAVE});
	}

	/**
	 * Factory function. Creates a file save config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default path of the file
	 * @param allowedExtensions The allowed extensions of the file to be saved
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileSaveOption(
		const std::string &description, const std::string &defaultValue, const std::string &allowedExtensions) {
		return getOption<_ConfigVariant::FILE>(description, defaultValue, {allowedExtensions, _FileDialogMode::SAVE});
	}

	/**
	 * Factory function. Creates directory choose option
	 * @param description A description that describes the purpose of this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption directoryOption(const std::string &description) {
		return getOption<_ConfigVariant::FILE>(description, "", {"", _FileDialogMode::DIRECTORY});
	}

	/**
	 * Factory function. Creates directory choose option
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default path of the directory
	 * @return A ConfigOption Object
	 */
	static ConfigOption directoryOption(const std::string &description, const std::string &defaultValue) {
		return getOption<_ConfigVariant::FILE>(description, defaultValue, {"", _FileDialogMode::DIRECTORY});
	}

	/**
	 * Factory function. Creates bool option
	 * @param description A description that describes the purpose of this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption boolOption(const std::string &description) {
		return getOption<_ConfigVariant::BOOLEAN>(description, false, {});
	}

	/**
	 * Factory function. Creates bool option
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value of the option
	 * @return A ConfigOption Object
	 */
	static ConfigOption boolOption(const std::string &description, const bool defaultValue) {
		return getOption<_ConfigVariant::BOOLEAN>(description, defaultValue, {});
	}
};

/**
 * A map of current config values at runtime. Extends std::map.
 * Type agnostic, enforces type only on accessing elements.
 */
class RuntimeConfigMap : public std::map<std::string, boost::any> {
public:
	/**
	 * Returns the config stored at the given key, casted to the
	 * given type.
	 * @tparam T The C++ type of the return value. Must be known.
	 * @param key The key at which to get the current config value
	 * @return The casted config value.
	 */
	template<class T> const T get(const std::string &key) {
		return boost::any_cast<T>(this->at(key));
	}

	/**
	 * Returns the config stored at the given key, casted to the
	 * given type.
	 * @tparam T The config variant of the accessed key. Must be known.
	 * @param key The key at which to get the current config value
	 * @return The casted config value.
	 */
	template<_ConfigVariant V> const typename _ConfigVariantType<V>::type get(const std::string &key) {
		return get<typename _ConfigVariantType<V>::type>(key);
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
	thread_local static caerModuleData __moduleData;

public:
	/**
	 * Static function that calls the user provided static function `T::getConfigOptions` and processes its output.
	 * For every config option, a node is generated in the caer configuration tree.
	 * @tparam T The subclass of T for which the config nodes should be generated.
	 * @param node The sshs node for which the config should be generated
	 */
	template<class T> static void configInit(sshsNode node) {
		static_assert(std::is_base_of<BaseModule, T>::value, "The provided type does not inherit from BaseModule");

		// read config options from static user provided function
		std::map<std::string, ConfigOption> configOptions;
		T::getConfigOptions(configOptions);

		for (auto const &entry : configOptions) {
			auto key    = entry.first;
			auto config = entry.second;
			switch (config.getVariant()) {
				case _ConfigVariant::FRACTIONAL: {
					auto config_ = config.getConfigObject<_ConfigVariant::FRACTIONAL>();
					sshsNodeCreateDouble(node, key.c_str(), config_.initValue, config_.attributes.min,
						config_.attributes.max, SSHS_FLAGS_NORMAL, config_.description.c_str());
					break;
				}
				case _ConfigVariant::INTEGER: {
					auto config_ = config.getConfigObject<_ConfigVariant::INTEGER>();
					sshsNodeCreateLong(node, key.c_str(), config_.initValue, config_.attributes.min,
						config_.attributes.max, SSHS_FLAGS_NORMAL, config_.description.c_str());
					break;
				}
				case _ConfigVariant::STRING: {
					auto config_ = config.getConfigObject<_ConfigVariant::STRING>();
					sshsNodeCreateString(node, key.c_str(), config_.initValue.c_str(), 0, UINT32_MAX, SSHS_FLAGS_NORMAL,
						config_.description.c_str());
					break;
				}
				case _ConfigVariant::FILE: {
					auto config_ = config.getConfigObject<_ConfigVariant::FILE>();
					sshsNodeCreateString(node, key.c_str(), config_.initValue.c_str(), 0, PATH_MAX, SSHS_FLAGS_NORMAL,
						config_.description.c_str());

					std::string fileChooserAttribute
						= (config_.attributes.mode == _FileDialogMode::OPEN)
							  ? "LOAD"
							  : ((config_.attributes.mode == _FileDialogMode::SAVE) ? "SAVE" : "DIRECTORY");
					sshsNodeCreateAttributeFileChooser(
						node, key.c_str(), fileChooserAttribute + ":" + config_.attributes.allowedExtensions);
					break;
				}
			}
		}
	}

	/**
	 * __INTERNAL USE ONLY__
	 * Sets the static, thread local module data to be used by
	 * a subsequent constructor. This shall only be used prior to
	 * @param _moduleData The moduleData param to be used for
	 * BaseModule member initialization upon constructor
	 */
	static void __setStaticModuleData(caerModuleData _moduleData) {
		__moduleData = _moduleData;
	}

	/**
	 * Logger object to be used in implementation
	 */

	Logger log;

	/**
	 * Map that contains the runtime config values of the configs.
	 */
	RuntimeConfigMap config;


	/**
	 * Base module constructor. The base module constructor initializes
	 * the logger and config members of the class, by utilizing the
	 * `static_thread` local pointer to the caer moduleData pointer
	 * provided prior to constructrion. This makes sure, that logger
	 * and config are available at the time the subclass constructor is
	 * called.
	 */
	BaseModule() : log(Logger(__moduleData)) {
	    assert(__moduleData);
	    configUpdate(__moduleData->moduleNode);
	}


	/**
	 * Method that updates the configs in the map as soon as some config
	 * change.
	 * @param node the sshs node to read the config from.
	 */
	void configUpdate(sshsNode node) {
		size_t nKeys;
		auto keys = sshsNodeGetAttributeKeys(node, &nKeys);
		for (size_t i = 0; i < nKeys; i++) {
			switch (sshsNodeGetAttributeType(node, keys[i])) {
				case SSHS_BOOL: {
					config[keys[i]] = sshsNodeGetBool(node, keys[i]);
					break;
				}
				case SSHS_INT: {
					config[keys[i]] = sshsNodeGetInt(node, keys[i]);
					break;
				}
				case SSHS_LONG: {
					config[keys[i]] = sshsNodeGetLong(node, keys[i]);
					break;
				}
				case SSHS_FLOAT: {
					config[keys[i]] = sshsNodeGetFloat(node, keys[i]);
					break;
				}
				case SSHS_DOUBLE: {
					config[keys[i]] = sshsNodeGetDouble(node, keys[i]);
					break;
				}
				case SSHS_STRING: {
					config[keys[i]] = sshsNodeGetStdString(node, keys[i]);
					break;
				}
				default: {}
			}
		}
	}

	/**
	 * Wrapper for the run function that wraps the packets into their C++
	 * representation.
	 * @param in The input libcaer packet
	 * @param out the output libcaer packet
	 */
	void runBase(caerEventPacketContainer in, caerEventPacketContainer *out) {
		auto in_ = libcaer::events::EventPacketContainer(in, false);
		// TODO: Handle the out behaviour
		run(in_);
	};

	/**
	 * Virtual function to be implemented by the user.
	 * @param in
	 *
	 */
	virtual void run(const libcaer::events::EventPacketContainer &in) = 0;
};

    /**
     * Instantiation of thread_local static moduleData pointer.
     * This pointer is set prior to construction to allow the constructor
     * to access relevant data.
     */
    thread_local caerModuleData BaseModule::__moduleData = nullptr;


} // namespace caer

#endif // CAER_MODULES_SDK_BASEMODULE_H
