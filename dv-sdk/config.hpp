#ifndef DV_CONFIG_HPP
#define DV_CONFIG_HPP

#include "module.h"

#include <cmath>
#include <map>
#include <memory>
#include <string>

namespace dv {

/**
 * Returns the sign of the given number as -1 or 1. Returns 1 for 0.
 * @tparam T The data type of the number and return value
 * @param x the data to be checked
 * @return -1 iff x < 0, 1 otherwise
 */
template<class T> inline T sgn(T x) {
	return (x < (T) 0) ? (T) -1 : (T) 1;
}

/**
 * Different opening modes for a File Dialog config option
 */
enum class _FileDialogMode { NONE, OPEN, SAVE, DIRECTORY };

/**
 * The different config variations a user can choose from, when building the
 * configuration of a module
 */
enum class ConfigVariant { NONE, BOOLEAN, FILE, STRING, INTEGER, FRACTIONAL };

/**
 * Maps the selected config variant template to a C++ data type.
 */
template<ConfigVariant> struct _ConfigVariantType {};
template<> struct _ConfigVariantType<ConfigVariant::BOOLEAN> { typedef bool type; };
template<> struct _ConfigVariantType<ConfigVariant::FILE> { typedef std::string type; };
template<> struct _ConfigVariantType<ConfigVariant::STRING> { typedef std::string type; };
template<> struct _ConfigVariantType<ConfigVariant::INTEGER> { typedef int64_t type; };
template<> struct _ConfigVariantType<ConfigVariant::FRACTIONAL> { typedef double type; };

/**
 * Maps the selected config variant to a struct with additional config params
 * that have to be set for the selected config variant. Defaults to an empty struct.
 */
template<ConfigVariant> struct _ConfigAttributes {};
template<> struct _ConfigAttributes<ConfigVariant::FILE> {
	std::string allowedExtensions;
	_FileDialogMode mode = _FileDialogMode::NONE;
};
template<> struct _ConfigAttributes<ConfigVariant::INTEGER> {
	using _VariantType = typename _ConfigVariantType<ConfigVariant::INTEGER>::type;
	_VariantType min;
	_VariantType max;
};
template<> struct _ConfigAttributes<ConfigVariant::FRACTIONAL> {
	using _VariantType = typename _ConfigVariantType<ConfigVariant::FRACTIONAL>::type;
	_VariantType min;
	_VariantType max;
};

/**
 * Templated implementation class of a ConfigOption. Stores in a layout according
 * to the selected config variant.
 * @tparam V
 */
template<ConfigVariant V> class _ConfigOption {
	using _VariantType = typename _ConfigVariantType<V>::type;

public:
	const std::string description;
	const _VariantType initValue;
	const _ConfigAttributes<V> attributes;
	_VariantType currentValue;
	_ConfigOption(const std::string &description_, _VariantType initValue_, const _ConfigAttributes<V> &attributes_) :
		description(description_),
		initValue(initValue_),
		attributes(attributes_),
		currentValue(initValue_) {
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
	ConfigVariant variant_   = ConfigVariant::NONE;
	bool dvConfigNodeCreated = false;

	/**
	 * __Private constructor__
	 * Takes shared_ptr to templated `_ConfigOption` as well as the variant.
	 * @param configOption A shared_ptr to an instantiated `_ConfigOption`
	 * @param variant The config variant of the passed option
	 */
	ConfigOption(std::shared_ptr<void> configOption, ConfigVariant variant) :
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
	template<ConfigVariant V>
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
	inline ConfigVariant getVariant() const {
		return variant_;
	}

	/**
	 * Returns the underlying config object, casted to the specified configVariant.
	 * The config variant can be read out with the `getVariant` method.
	 * @tparam V The config variant to be casted to.
	 * @return The underlying _ConfigObject with the configuration data
	 */
	template<ConfigVariant V> _ConfigOption<V> &getConfigObject() const {
		return *(std::static_pointer_cast<_ConfigOption<V>>(configOption_));
	}

	/**
	 * Returns the current value of this config option. Needs a template paramenter
	 * of the type `dv::ConfigVariant::*` to determine what type of config parameter
	 * to return.
	 * @tparam V The config variant type
	 * @return A simple value (long, string etc) that is the current value of the config option
	 */
	template<ConfigVariant V> typename _ConfigVariantType<V>::type &getValue() const {
		return (typename _ConfigVariantType<V>::type &) (getConfigObject<V>().currentValue);
	}

	/**
	 * Creates a dvConfig Attribute in the dv config tree for the object.
	 * If a the node already has been created, nothing is done.
	 * @param key the key under which the new attribute should get stored
	 * @param node The dvConfigNode under which the new attribute shall be created
	 */
	void createDvConfigNodeIfChanged(const std::string &key, dvConfigNode node) {
		if (dvConfigNodeCreated) {
			return;
		}

		switch (variant_) {
			case ConfigVariant::BOOLEAN: {
				auto &config_ = getConfigObject<ConfigVariant::BOOLEAN>();
				dvConfigNodeCreateBool(
					node, key.c_str(), config_.initValue, DVCFG_FLAGS_NORMAL, config_.description.c_str());
				break;
			}
			case ConfigVariant::FRACTIONAL: {
				auto &config_ = getConfigObject<ConfigVariant::FRACTIONAL>();
				dvConfigNodeCreateDouble(node, key.c_str(), config_.initValue, config_.attributes.min,
					config_.attributes.max, DVCFG_FLAGS_NORMAL, config_.description.c_str());
				break;
			}
			case ConfigVariant::INTEGER: {
				auto &config_ = getConfigObject<ConfigVariant::INTEGER>();
				dvConfigNodeCreateLong(node, key.c_str(), config_.initValue, config_.attributes.min,
					config_.attributes.max, DVCFG_FLAGS_NORMAL, config_.description.c_str());
				break;
			}
			case ConfigVariant::STRING: {
				auto &config_ = getConfigObject<ConfigVariant::STRING>();
				dvConfigNodeCreateString(node, key.c_str(), config_.initValue.c_str(), 0, INT32_MAX, DVCFG_FLAGS_NORMAL,
					config_.description.c_str());
				break;
			}
			case ConfigVariant::FILE: {
				auto &config_ = getConfigObject<ConfigVariant::FILE>();
				dvConfigNodeCreateString(node, key.c_str(), config_.initValue.c_str(), 0, PATH_MAX, DVCFG_FLAGS_NORMAL,
					config_.description.c_str());

				std::string fileChooserAttribute
					= (config_.attributes.mode == _FileDialogMode::OPEN)
						  ? "LOAD"
						  : ((config_.attributes.mode == _FileDialogMode::SAVE) ? "SAVE" : "DIRECTORY");
				dvConfigNodeAttributeModifierFileChooser(
					node, key.c_str(), (fileChooserAttribute + ":" + config_.attributes.allowedExtensions).c_str());
				break;
			}
			case ConfigVariant::NONE: {
				break;
			}
		}
		dvConfigNodeCreated = true;
	}

	/**
	 * Updates the current value of the ConfigOption based on the value
	 * that is present in the dv config tree.
	 * @param key The key under which to find the value in the dv config tree.
	 * @param node The node of the dv config treee under which the attribute is to be found.
	 */
	void updateValue(const std::string &key, dvConfigNode node) {
		switch (variant_) {
			case ConfigVariant::BOOLEAN: {
				auto &config_        = getConfigObject<ConfigVariant::BOOLEAN>();
				config_.currentValue = dvConfigNodeGetBool(node, key.c_str());
			}
			case ConfigVariant::FRACTIONAL: {
				auto &config_        = getConfigObject<ConfigVariant::FRACTIONAL>();
				config_.currentValue = dvConfigNodeGetDouble(node, key.c_str());
				break;
			}
			case ConfigVariant::INTEGER: {
				auto &config_        = getConfigObject<ConfigVariant::INTEGER>();
				config_.currentValue = dvConfigNodeGetLong(node, key.c_str());
				break;
			}
			case ConfigVariant::STRING: {
				auto &config_        = getConfigObject<ConfigVariant::STRING>();
				config_.currentValue = dvConfigNodeGetString(node, key.c_str());
				break;
			}
			case ConfigVariant::FILE: {
				auto &config_        = getConfigObject<ConfigVariant::FILE>();
				config_.currentValue = dvConfigNodeGetString(node, key.c_str());
				break;
			}
			case ConfigVariant::NONE: {
				break;
			}
		}
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
		return getOption<ConfigVariant::FRACTIONAL>(description, defaultValue, {minValue, maxValue});
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
		return getOption<ConfigVariant::FRACTIONAL>(description, defaultValue, {0., sensibleUpperRange});
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
		return getOption<ConfigVariant::INTEGER>(description, defaultValue, {minValue, maxValue});
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
		return getOption<ConfigVariant::INTEGER>(description, defaultValue, {0, sensibleUpperRange});
	}

	/**
	 * Factory function. Creates a string config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @return A ConfigOption Object
	 */
	static ConfigOption stringOption(const std::string &description, const std::string &defaultValue) {
		return getOption<ConfigVariant::STRING>(description, defaultValue, {});
	}

	/**
	 * Factory function. Creates a file open config option.
	 * @param description A description that describes the purpose of this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileOpenOption(const std::string &description) {
		return getOption<ConfigVariant::FILE>(description, "", {".*", _FileDialogMode::OPEN});
	}

	/**
	 * Factory function. Creates a file open config option.
	 * @param description A description that describes the purpose of this option
	 * @param allowedExtensions The allowed extensions of the file to be opened
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileOpenOption(const std::string &description, const std::string &allowedExtensions) {
		return getOption<ConfigVariant::FILE>(description, "", {allowedExtensions, _FileDialogMode::OPEN});
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
		return getOption<ConfigVariant::FILE>(description, defaultValue, {allowedExtensions, _FileDialogMode::OPEN});
	}

	/**
	 * Factory function. Creates a file save config option.
	 * @param description A description that describes the purpose of this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileSaveOption(const std::string &description) {
		return getOption<ConfigVariant::FILE>(description, "", {"*", _FileDialogMode::SAVE});
	}

	/**
	 * Factory function. Creates a file save config option.
	 * @param description A description that describes the purpose of this option
	 * @param allowedExtensions The allowed extensions of the file to be saved
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileSaveOption(const std::string &description, const std::string &allowedExtensions) {
		return getOption<ConfigVariant::FILE>(description, "", {allowedExtensions, _FileDialogMode::SAVE});
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
		return getOption<ConfigVariant::FILE>(description, defaultValue, {allowedExtensions, _FileDialogMode::SAVE});
	}

	/**
	 * Factory function. Creates directory choose option
	 * @param description A description that describes the purpose of this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption directoryOption(const std::string &description) {
		return getOption<ConfigVariant::FILE>(description, "", {"", _FileDialogMode::DIRECTORY});
	}

	/**
	 * Factory function. Creates directory choose option
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default path of the directory
	 * @return A ConfigOption Object
	 */
	static ConfigOption directoryOption(const std::string &description, const std::string &defaultValue) {
		return getOption<ConfigVariant::FILE>(description, defaultValue, {"", _FileDialogMode::DIRECTORY});
	}

	/**
	 * Factory function. Creates bool option
	 * @param description A description that describes the purpose of this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption boolOption(const std::string &description) {
		return getOption<ConfigVariant::BOOLEAN>(description, false, {});
	}

	/**
	 * Factory function. Creates bool option
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value of the option
	 * @return A ConfigOption Object
	 */
	static ConfigOption boolOption(const std::string &description, const bool defaultValue) {
		return getOption<ConfigVariant::BOOLEAN>(description, defaultValue, {});
	}
};

/**
 * A map of current config values at runtime. Extends std::map.
 * Type agnostic, enforces type only on accessing elements.
 */
class RuntimeConfigMap : public std::map<std::string, ConfigOption> {
public:
	/**
	 * Returns the underlying `_ConfigOption` object for the given key. The
	 * config option provides access to parameters and default and current values.
	 * @tparam V The type of the config param. Type of `dv::ConfigVariant::`
	 * @param key The key of the config option to retrieve
	 * @return The underlying `_ConfigObject` of the config option. Gives access to parameters.
	 */
	template<ConfigVariant V> _ConfigOption<V> const &getConfigObject(const std::string &key) {
		return this->at(key).getConfigObject<V>();
	}

	/**
	 * Returns the current value of the config option with the given key. Needs a template paramenter
	 * of the type `dv::ConfigVariant::*` to determine what type of config parameter
	 * to return.
	 * @tparam V The config variant type
	 * @param key the key of the config option to look up
	 * @return A simple value (long, string etc) that is the current value of the config option
	 */
	template<ConfigVariant V> const typename _ConfigVariantType<V>::type getValue(const std::string &key) {
		return (typename _ConfigVariantType<V>::type)(getConfigObject<V>(key).currentValue);
	}
};
} // namespace dv

#endif // DV_CONFIG_HPP
