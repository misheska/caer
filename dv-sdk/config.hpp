#ifndef DV_SDK_CONFIG_HPP
#define DV_SDK_CONFIG_HPP

#include "cross/portable_io.h"
#include "utils.h"

#include <boost/algorithm/string/join.hpp>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace dv {

/**
 * Returns the sign of the given number as -1 or 1. Returns 1 for 0.
 * @tparam T The data type of the number and return value
 * @param x the data to be checked
 * @return -1 iff x < 0, 1 otherwise
 */
template<class T> inline T sgn(T x) {
	return (x < static_cast<T>(0)) ? static_cast<T>(-1) : static_cast<T>(1);
}

/**
 * Different viewing modes for a Button (boolean) config option.
 * NONE is a normal checkbox.
 */
enum class ButtonMode { NONE, PLAY, ONOFF, EXECUTE };

/**
 * Different opening modes for a File Dialog config option.
 */
enum class FileDialogMode { OPEN, SAVE, DIRECTORY };

/**
 * INTERNAL: select between different string types.
 */
enum class _StringAttributeType { NORMAL, LIST, FILE };

/**
 * Maps the selected config type to a struct with additional config params
 * that have to be set for the selected config type. Defaults to an empty struct.
 */
template<dv::Config::AttributeType> struct _ConfigAttributes {};
template<> struct _ConfigAttributes<dv::Config::AttributeType::BOOL> {
	dv::ButtonMode mode;
	bool autoReset;

	_ConfigAttributes(ButtonMode bm, bool arst = false) : mode(bm), autoReset(arst) {
	}
};
template<> struct _ConfigAttributes<dv::Config::AttributeType::INT> {
	dv::Config::AttributeRanges<dv::Config::AttributeType::INT> range;
	std::string unit;

	_ConfigAttributes(int32_t minValue, int32_t maxValue) : range(minValue, maxValue) {
	}
};
template<> struct _ConfigAttributes<dv::Config::AttributeType::LONG> {
	dv::Config::AttributeRanges<dv::Config::AttributeType::LONG> range;
	std::string unit;

	_ConfigAttributes(int64_t minValue, int64_t maxValue) : range(minValue, maxValue) {
	}
};
template<> struct _ConfigAttributes<dv::Config::AttributeType::FLOAT> {
	dv::Config::AttributeRanges<dv::Config::AttributeType::FLOAT> range;
	std::string unit;

	_ConfigAttributes(float minValue, float maxValue) : range(minValue, maxValue) {
	}
};
template<> struct _ConfigAttributes<dv::Config::AttributeType::DOUBLE> {
	dv::Config::AttributeRanges<dv::Config::AttributeType::DOUBLE> range;
	std::string unit;

	_ConfigAttributes(double minValue, double maxValue) : range(minValue, maxValue) {
	}
};
template<> struct _ConfigAttributes<dv::Config::AttributeType::STRING> {
	dv::Config::AttributeRanges<dv::Config::AttributeType::STRING> length;
	_StringAttributeType type;
	// List of strings related options.
	std::vector<std::string> listOptions;
	bool listAllowMultipleSelections;
	// File chooser (path string) related options.
	FileDialogMode fileMode;
	std::string fileAllowedExtensions;

	_ConfigAttributes(int32_t minLength, int32_t maxLength, _StringAttributeType t) :
		length(minLength, maxLength),
		type(t) {
	}
};

/**
 * Templated implementation class of a ConfigOption. Stores extra attributes according to the selected config type.
 */
template<dv::Config::AttributeType T> class _ConfigOption {
	using _AttrType = typename dv::Config::AttributeTypeGenerator<T>::type;

public:
	const std::string description;
	const _AttrType initValue;
	const _ConfigAttributes<T> attributes;
	const dv::Config::AttributeFlags flags;
	_AttrType currentValue;

	_ConfigOption(const std::string &description_, _AttrType initValue_, const _ConfigAttributes<T> &attributes_,
		dv::Config::AttributeFlags flags_) :
		description(description_),
		initValue(initValue_),
		attributes(attributes_),
		flags(flags_),
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
	dv::unique_ptr_void configOption;
	dv::Config::AttributeType type;

	/**
	 * __Private constructor__
	 * Takes shared_ptr to templated `_ConfigOption` as well as the variant.
	 * @param configOption A shared_ptr to an instantiated `_ConfigOption`
	 * @param variant The config variant of the passed option
	 */
	ConfigOption(dv::unique_ptr_void configOption_, dv::Config::AttributeType type_) :
		configOption(std::move(configOption_)),
		type(type_) {
	}

	/**
	 * Private base factory method.
	 * Used as a base for all the config factories defined
	 * Creates a new ConfigOption of the requested type. Works by first instantiating
	 * a `_ConfigOption` with the right templated variant, then creating a `shared_ptr`
	 * and creating a `ConfigOption` to return.
	 * @tparam T
	 * @param description
	 * @param defaultValue
	 * @param attributes_
	 * @return
	 */
	template<dv::Config::AttributeType T>
	static ConfigOption getOption(const std::string &description,
		typename dv::Config::AttributeTypeGenerator<T>::type defaultValue, const _ConfigAttributes<T> &attributes,
		dv::Config::AttributeFlags flags) {
		return (
			ConfigOption(dv::make_unique_void(new _ConfigOption<T>(description, defaultValue, attributes, flags)), T));
	}

public:
	/**
	 * Returns the type of this `ConfigOption`.
	 * @return
	 */
	dv::Config::AttributeType getType() const {
		return (type);
	}

	/**
	 * Returns the underlying config object, casted to the specified configVariant.
	 * The config variant can be read out with the `getVariant` method.
	 * @tparam T The config variant to be casted to.
	 * @return The underlying _ConfigObject with the configuration data
	 */
	template<dv::Config::AttributeType T> _ConfigOption<T> &getConfigObject() {
		return (*(static_cast<_ConfigOption<T> *>(configOption.get())));
	}

	/**
	 * Returns the underlying config object, casted to the specified configVariant.
	 * The config variant can be read out with the `getVariant` method.
	 * @tparam T The config variant to be casted to.
	 * @return The underlying _ConfigObject with the configuration data
	 */
	template<dv::Config::AttributeType T> _ConfigOption<T> const &getConfigObject() const {
		return (*(static_cast<_ConfigOption<T> const *>(configOption.get())));
	}

	/**
	 * Returns the current value of this config option. Needs a template paramenter
	 * of the type `dv::ConfigVariant::*` to determine what type of config parameter
	 * to return.
	 * @tparam T The config variant type
	 * @return A simple value (long, string etc) that is the current value of the config option
	 */
	template<dv::Config::AttributeType T> typename dv::Config::AttributeTypeGenerator<T>::type const &get() const {
		return (static_cast<typename dv::Config::AttributeTypeGenerator<T>::type const &>(
			getConfigObject<T>().currentValue));
	}

	/**
	 * Creates a dvConfig Attribute in the dv config tree for the object.
	 * If a the node already has been created, nothing is done.
	 * @param key the key under which the new attribute should get stored
	 * @param node The dvConfigNode under which the new attribute shall be created
	 */
	void createAttribute(const std::string &fullKey, dv::Config::Node moduleNode) {
		dv::Config::Node node = moduleNode;
		std::string key;

		size_t pos = fullKey.find_last_of("/");

		if (pos != std::string::npos) {
			node = moduleNode.getRelativeNode(fullKey.substr(0, pos + 1));
			key  = fullKey.substr(pos + 1);
		}
		else {
			// node is already moduleNode.
			key = fullKey;
		}

		switch (type) {
			case dv::Config::AttributeType::BOOL: {
				auto &config = getConfigObject<dv::Config::AttributeType::BOOL>();

				node.createAttribute<dv::Config::AttributeType::BOOL>(
					key, config.initValue, {}, config.flags, config.description);

				if (config.attributes.mode != dv::ButtonMode::NONE) {
					if (config.attributes.mode == dv::ButtonMode::PLAY) {
						node.attributeModifierButton(key, "PLAY");
					}
					else if (config.attributes.mode == dv::ButtonMode::ONOFF) {
						node.attributeModifierButton(key, "ONOFF");
					}
					else {
						node.attributeModifierButton(key, "EXECUTE");
					}
				}

				if (config.attributes.autoReset) {
					node.attributeButtonReset(key);
				}

				break;
			}

			case dv::Config::AttributeType::INT: {
				auto &config = getConfigObject<dv::Config::AttributeType::INT>();

				node.createAttribute<dv::Config::AttributeType::INT>(
					key, config.initValue, config.attributes.range, config.flags, config.description);

				if (!config.attributes.unit.empty()) {
					node.attributeModifierUnit(key, config.attributes.unit);
				}

				break;
			}

			case dv::Config::AttributeType::LONG: {
				auto &config = getConfigObject<dv::Config::AttributeType::LONG>();

				node.createAttribute<dv::Config::AttributeType::LONG>(
					key, config.initValue, config.attributes.range, config.flags, config.description);

				if (!config.attributes.unit.empty()) {
					node.attributeModifierUnit(key, config.attributes.unit);
				}

				break;
			}

			case dv::Config::AttributeType::FLOAT: {
				auto &config = getConfigObject<dv::Config::AttributeType::FLOAT>();

				node.createAttribute<dv::Config::AttributeType::FLOAT>(
					key, config.initValue, config.attributes.range, config.flags, config.description);

				if (!config.attributes.unit.empty()) {
					node.attributeModifierUnit(key, config.attributes.unit);
				}

				break;
			}

			case dv::Config::AttributeType::DOUBLE: {
				auto &config = getConfigObject<dv::Config::AttributeType::DOUBLE>();

				node.createAttribute<dv::Config::AttributeType::DOUBLE>(
					key, config.initValue, config.attributes.range, config.flags, config.description);

				if (!config.attributes.unit.empty()) {
					node.attributeModifierUnit(key, config.attributes.unit);
				}

				break;
			}

			case dv::Config::AttributeType::STRING: {
				auto &config = getConfigObject<dv::Config::AttributeType::STRING>();

				node.createAttribute<dv::Config::AttributeType::STRING>(key,
					static_cast<const std::string_view>(config.initValue), config.attributes.length, config.flags,
					config.description);

				if (config.attributes.type == _StringAttributeType::LIST) {
					std::string listAttribute = boost::algorithm::join(config.attributes.listOptions, ",");

					node.attributeModifierListOptions(
						key, listAttribute, config.attributes.listAllowMultipleSelections);
				}

				if (config.attributes.type == _StringAttributeType::FILE) {
					std::string fileChooserAttribute;

					if (config.attributes.fileMode == FileDialogMode::OPEN) {
						fileChooserAttribute += "LOAD";
					}
					else if (config.attributes.fileMode == FileDialogMode::SAVE) {
						fileChooserAttribute += "SAVE";
					}
					else {
						fileChooserAttribute += "DIRECTORY";
					}

					if (!config.attributes.fileAllowedExtensions.empty()) {
						fileChooserAttribute.push_back(':');
						fileChooserAttribute += config.attributes.fileAllowedExtensions;
					}

					node.attributeModifierFileChooser(key, fileChooserAttribute);
				}

				break;
			}

			case dv::Config::AttributeType::UNKNOWN: {
				break;
			}
		}
	}

	/**
	 * Updates the current value of the ConfigOption based on the value
	 * that is present in the dv config tree.
	 * @param key The key under which to find the value in the dv config tree.
	 * @param node The node of the dv config treee under which the attribute is to be found.
	 */
	void updateValue(const std::string &fullKey, dv::Config::Node moduleNode) {
		dv::Config::Node node = moduleNode;
		std::string key;

		size_t pos = fullKey.find_last_of("/");

		if (pos != std::string::npos) {
			node = moduleNode.getRelativeNode(fullKey.substr(0, pos + 1));
			key  = fullKey.substr(pos + 1);
		}
		else {
			// node is already moduleNode.
			key = fullKey;
		}

		switch (type) {
			case dv::Config::AttributeType::BOOL: {
				auto &config = getConfigObject<dv::Config::AttributeType::BOOL>();

				config.currentValue = node.get<dv::Config::AttributeType::BOOL>(key);

				break;
			}

			case dv::Config::AttributeType::INT: {
				auto &config = getConfigObject<dv::Config::AttributeType::INT>();

				config.currentValue = node.get<dv::Config::AttributeType::INT>(key);

				break;
			}

			case dv::Config::AttributeType::LONG: {
				auto &config = getConfigObject<dv::Config::AttributeType::LONG>();

				config.currentValue = node.get<dv::Config::AttributeType::LONG>(key);

				break;
			}

			case dv::Config::AttributeType::FLOAT: {
				auto &config = getConfigObject<dv::Config::AttributeType::FLOAT>();

				config.currentValue = node.get<dv::Config::AttributeType::FLOAT>(key);

				break;
			}

			case dv::Config::AttributeType::DOUBLE: {
				auto &config = getConfigObject<dv::Config::AttributeType::DOUBLE>();

				config.currentValue = node.get<dv::Config::AttributeType::DOUBLE>(key);

				break;
			}

			case dv::Config::AttributeType::STRING: {
				auto &config = getConfigObject<dv::Config::AttributeType::STRING>();

				config.currentValue = node.get<dv::Config::AttributeType::STRING>(key);

				break;
			}

			case dv::Config::AttributeType::UNKNOWN: {
				break;
			}
		}
	}

	// Static convenience factory methods -------------------------------------------------------
	/**
	 * Factory function. Creates boolean option (checkbox).
	 * @param description A description that describes the purpose of this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption boolOption(const std::string &description) {
		return getOption<dv::Config::AttributeType::BOOL>(
			description, false, {dv::ButtonMode::NONE}, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates boolean option (checkbox).
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value of the option
	 * @return A ConfigOption Object
	 */
	static ConfigOption boolOption(const std::string &description, bool defaultValue) {
		return getOption<dv::Config::AttributeType::BOOL>(
			description, defaultValue, {dv::ButtonMode::NONE}, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates boolean option (checkbox).
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value of the option
	 * @return A ConfigOption Object
	 */
	static ConfigOption boolOption(const std::string &description, bool defaultValue, const dv::ButtonMode &mode) {
		return getOption<dv::Config::AttributeType::BOOL>(
			description, defaultValue, {mode}, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a integer config option (32 bit).
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @param minValue The min value a user can choose for this option
	 * @param maxValue The max value a user can choose for this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption intOption(
		const std::string &description, int32_t defaultValue, int32_t minValue, int32_t maxValue) {
		return getOption<dv::Config::AttributeType::INT>(
			description, defaultValue, {minValue, maxValue}, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a integer config option (32 bit).
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @return A ConfigOption Object
	 */
	static ConfigOption intOption(const std::string &description, int32_t defaultValue) {
		int32_t sensibleUpperRange
			= ((std::abs(defaultValue) > 0)
					  ? static_cast<int32_t>(
							std::pow(10.0, std::floor(std::log10(static_cast<double>(std::abs(defaultValue))) + 1.0)))
					  : 1)
			  * sgn(defaultValue);

		return getOption<dv::Config::AttributeType::INT>(
			description, defaultValue, {0, sensibleUpperRange}, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a long integer config option (64 bit).
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @param minValue The min value a user can choose for this option
	 * @param maxValue The max value a user can choose for this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption longOption(
		const std::string &description, int64_t defaultValue, int64_t minValue, int64_t maxValue) {
		return getOption<dv::Config::AttributeType::LONG>(
			description, defaultValue, {minValue, maxValue}, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a long integer config option (64 bit).
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @return A ConfigOption Object
	 */
	static ConfigOption longOption(const std::string &description, int64_t defaultValue) {
		int64_t sensibleUpperRange
			= ((std::abs(defaultValue) > 0)
					  ? static_cast<int64_t>(
							std::pow(10.0, std::floor(std::log10(static_cast<double>(std::abs(defaultValue))) + 1.0)))
					  : 1)
			  * sgn(defaultValue);

		return getOption<dv::Config::AttributeType::LONG>(
			description, defaultValue, {0, sensibleUpperRange}, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a single-precision floating point config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @param minValue The min value a user can choose for this option
	 * @param maxValue The max value a user can choose for this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption floatOption(
		const std::string &description, float defaultValue, float minValue, float maxValue) {
		return getOption<dv::Config::AttributeType::FLOAT>(
			description, defaultValue, {minValue, maxValue}, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a single-precision floating point config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @return A ConfigOption Object
	 */
	static ConfigOption floatOption(const std::string &description, float defaultValue) {
		float sensibleUpperRange
			= ((std::abs(defaultValue) > 0.0f) ? std::pow(10.0f, std::floor(std::log10(std::abs(defaultValue)) + 1.0f))
											   : 1.0f)
			  * sgn(defaultValue);

		return getOption<dv::Config::AttributeType::FLOAT>(
			description, defaultValue, {0.0f, sensibleUpperRange}, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a double-precision floating point config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @param minValue The min value a user can choose for this option
	 * @param maxValue The max value a user can choose for this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption doubleOption(
		const std::string &description, double defaultValue, double minValue, double maxValue) {
		return getOption<dv::Config::AttributeType::DOUBLE>(
			description, defaultValue, {minValue, maxValue}, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a double-precision floating point config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @return A ConfigOption Object
	 */
	static ConfigOption doubleOption(const std::string &description, double defaultValue) {
		double sensibleUpperRange
			= ((std::abs(defaultValue) > 0.0) ? std::pow(10.0, std::floor(std::log10(std::abs(defaultValue)) + 1.0))
											  : 1.0)
			  * sgn(defaultValue);

		return getOption<dv::Config::AttributeType::DOUBLE>(
			description, defaultValue, {0.0, sensibleUpperRange}, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a string config option.
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default value that this option shall have
	 * @return A ConfigOption Object
	 */
	static ConfigOption stringOption(const std::string &description, const std::string &defaultValue) {
		return getOption<dv::Config::AttributeType::STRING>(description, defaultValue,
			{0, INT32_MAX, _StringAttributeType::NORMAL}, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a file open config option.
	 * @param description A description that describes the purpose of this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileOpenOption(const std::string &description) {
		_ConfigAttributes<dv::Config::AttributeType::STRING> attr{0, PATH_MAX, _StringAttributeType::FILE};
		attr.fileMode              = FileDialogMode::OPEN;
		attr.fileAllowedExtensions = "";

		return getOption<dv::Config::AttributeType::STRING>(description, "", attr, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a file open config option.
	 * @param description A description that describes the purpose of this option
	 * @param allowedExtensions The allowed extensions of the file to be opened
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileOpenOption(const std::string &description, const std::string &allowedExtensions) {
		_ConfigAttributes<dv::Config::AttributeType::STRING> attr{0, PATH_MAX, _StringAttributeType::FILE};
		attr.fileMode              = FileDialogMode::OPEN;
		attr.fileAllowedExtensions = allowedExtensions;

		return getOption<dv::Config::AttributeType::STRING>(description, "", attr, dv::Config::AttributeFlags::NORMAL);
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
		_ConfigAttributes<dv::Config::AttributeType::STRING> attr{0, PATH_MAX, _StringAttributeType::FILE};
		attr.fileMode              = FileDialogMode::OPEN;
		attr.fileAllowedExtensions = allowedExtensions;

		return getOption<dv::Config::AttributeType::STRING>(
			description, defaultValue, attr, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a file save config option.
	 * @param description A description that describes the purpose of this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileSaveOption(const std::string &description) {
		_ConfigAttributes<dv::Config::AttributeType::STRING> attr{0, PATH_MAX, _StringAttributeType::FILE};
		attr.fileMode              = FileDialogMode::SAVE;
		attr.fileAllowedExtensions = "";

		return getOption<dv::Config::AttributeType::STRING>(description, "", attr, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates a file save config option.
	 * @param description A description that describes the purpose of this option
	 * @param allowedExtensions The allowed extensions of the file to be saved
	 * @return A ConfigOption Object
	 */
	static ConfigOption fileSaveOption(const std::string &description, const std::string &allowedExtensions) {
		_ConfigAttributes<dv::Config::AttributeType::STRING> attr{0, PATH_MAX, _StringAttributeType::FILE};
		attr.fileMode              = FileDialogMode::SAVE;
		attr.fileAllowedExtensions = allowedExtensions;

		return getOption<dv::Config::AttributeType::STRING>(description, "", attr, dv::Config::AttributeFlags::NORMAL);
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
		_ConfigAttributes<dv::Config::AttributeType::STRING> attr{0, PATH_MAX, _StringAttributeType::FILE};
		attr.fileMode              = FileDialogMode::SAVE;
		attr.fileAllowedExtensions = allowedExtensions;

		return getOption<dv::Config::AttributeType::STRING>(
			description, defaultValue, attr, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates directory choose option
	 * @param description A description that describes the purpose of this option
	 * @return A ConfigOption Object
	 */
	static ConfigOption directoryOption(const std::string &description) {
		_ConfigAttributes<dv::Config::AttributeType::STRING> attr{0, PATH_MAX, _StringAttributeType::FILE};
		attr.fileMode = FileDialogMode::DIRECTORY;

		return getOption<dv::Config::AttributeType::STRING>(description, "", attr, dv::Config::AttributeFlags::NORMAL);
	}

	/**
	 * Factory function. Creates directory choose option
	 * @param description A description that describes the purpose of this option
	 * @param defaultValue The default path of the directory
	 * @return A ConfigOption Object
	 */
	static ConfigOption directoryOption(const std::string &description, const std::string &defaultValue) {
		_ConfigAttributes<dv::Config::AttributeType::STRING> attr{0, PATH_MAX, _StringAttributeType::FILE};
		attr.fileMode = FileDialogMode::DIRECTORY;

		return getOption<dv::Config::AttributeType::STRING>(
			description, defaultValue, attr, dv::Config::AttributeFlags::NORMAL);
	}
};

class RuntimeConfig {
private:
	std::unordered_map<std::string, ConfigOption> configMap;
	dv::Config::Node moduleNode;

public:
	RuntimeConfig(dv::Config::Node mn) : moduleNode(mn) {
	}

	void add(const std::string &key, ConfigOption cfg) {
		configMap.insert_or_assign(key, std::move(cfg));
		configMap[key].createAttribute(key, moduleNode);

		// Ensure value is up-to-date.
		configMap[key].updateValue(key, moduleNode);
	}

	template<dv::Config::AttributeType T>
	typename dv::Config::AttributeTypeGenerator<T>::type const &get(const std::string &key) const {
		auto &val = configMap.at(key);

#ifndef NDEBUG
		if (val.getType() != T) {
			throw std::runtime_error(
				"RuntimeConfig.get(" + key + "): key type and given template type are not the same.");
		}
#endif

		return (val.get<T>());
	}

	auto begin() {
		return (configMap.begin());
	}

	auto end() {
		return (configMap.end());
	}

	auto cbegin() const {
		return (configMap.cbegin());
	}

	auto cend() const {
		return (configMap.cend());
	}
};

} // namespace dv

#endif // DV_SDK_CONFIG_HPP
