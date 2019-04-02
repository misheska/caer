#include <libcaercpp/filters/dvs_noise.hpp>

#include "dv-sdk/module.hpp"

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

static union dvConfigAttributeValue updateHotPixelFiltered(
	void *userData, const char *key, enum dvConfigAttributeType type);
static union dvConfigAttributeValue updateBackgroundActivityFiltered(
	void *userData, const char *key, enum dvConfigAttributeType type);
static union dvConfigAttributeValue updateRefractoryPeriodFiltered(
	void *userData, const char *key, enum dvConfigAttributeType type);
static void caerDVSNoiseFilterConfigCustom(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

class DVSNoiseFilter : public dv::BaseModule {
private:
	dv::unique_ptr_deleter<struct caer_filter_dvs_noise> noiseFilter;

public:
	static void addInputs(std::vector<dv::InputDefinition> &in) {
		in.emplace_back("polarity", "POLA", false);
	}

	static void addOutputs(std::vector<dv::OutputDefinition> &out) {
		out.emplace_back("polarity", "POLA");
	}

	static const char *getDescription() {
		return ("Filters out noise from DVS change events.");
	}

	static void getConfigOptions(std::unordered_map<std::string, dv::ConfigOption> &config) {
		config["hotPixelLearn"] = dv::ConfigOption::boolOption(
			"Learn the position of current hot (abnormally active) pixels, so they can be filtered out.", false,
			dv::ButtonMode::EXECUTE);
		config["hotPixelTime"] = dv::ConfigOption::intOption(
			"Time in µs to accumulate events for learning new hot pixels.", 1000000, 0, 30000000);
		config["hotPixelCount"] = dv::ConfigOption::intOption(
			"Number of events needed in a learning time period for a pixel to be considered hot.", 10000, 0, 10000000);

		config["hotPixelEnable"] = dv::ConfigOption::boolOption("Enable the hot pixel filter.", false);

		config["backgroundActivityEnable"]
			= dv::ConfigOption::boolOption("Enable the background activity filter.", true);
		config["backgroundActivityTwoLevels"]
			= dv::ConfigOption::boolOption("Use two-level background activity filtering.");
		config["backgroundActivityCheckPolarity"]
			= dv::ConfigOption::boolOption("Consider polarity when filtering background activity.");
		config["backgroundActivitySupportMin"] = dv::ConfigOption::intOption(
			"Minimum number of direct neighbor pixels that must support this pixel for it to be valid.", 1, 1, 8);
		config["backgroundActivitySupportMax"] = dv::ConfigOption::intOption(
			"Maximum number of direct neighbor pixels that can support this pixel for it to be valid.", 8, 1, 8);
		config["backgroundActivityTime"] = dv::ConfigOption::intOption(
			"Maximum time difference in µs for events to be considered correlated and not be filtered out.", 2000, 0,
			10000000);

		config["refractoryPeriodEnable"] = dv::ConfigOption::boolOption("Enable the refractory period filter.", true);
		config["refractoryPeriodTime"]
			= dv::ConfigOption::intOption("Minimum time between events to not be filtered out.", 100, 0, 10000000);
	}

	static void advancedStaticInit(dv::Config::Node moduleNode) {
		moduleNode.create<dvCfgType::LONG>("hotPixelFiltered", 0, {0, INT64_MAX},
			dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Number of events filtered out by the hot pixel filter.");

		moduleNode.create<dvCfgType::LONG>("backgroundActivityFiltered", 0, {0, INT64_MAX},
			dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT,
			"Number of events filtered out by the background activity filter.");

		moduleNode.create<dvCfgType::LONG>("refractoryPeriodFiltered", 0, {0, INT64_MAX},
			dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT,
			"Number of events filtered out by the refractory period filter.");
	}

	DVSNoiseFilter() : noiseFilter(nullptr, [](struct caer_filter_dvs_noise *) {}) {
		// Wait for input to be ready. All inputs, once they are up and running, will
		// have a valid sourceInfo node to query, especially if dealing with data.
		// Allocate map using info from sourceInfo.
		auto info = inputGetInfoNode("polarity");
		if (!info) {
			throw std::runtime_error("Polarity input not ready, upstream module not running.");
		}

		int32_t sizeX = info.get<dvCfgType::INT>("sizeX");
		int32_t sizeY = info.get<dvCfgType::INT>("sizeY");

		noiseFilter = dv::unique_ptr_deleter<struct caer_filter_dvs_noise>(
			caerFilterDVSNoiseInitialize(U16T(sizeX), U16T(sizeY)),
			[](struct caer_filter_dvs_noise *f) { caerFilterDVSNoiseDestroy(f); });
		if (!noiseFilter) {
			throw std::runtime_error("Failed to initialize DVS Noise filter.");
		}

		caerFilterDVSNoiseConfigSet(
			noiseFilter.get(), CAER_FILTER_DVS_LOG_LEVEL, U64T(config["logLevel"].getValue<dvCfgType::INT>()));

		moduleNode.attributeUpdaterAdd(
			"hotPixelFiltered", dvCfgType::LONG, &updateHotPixelFiltered, noiseFilter.get(), false);
		moduleNode.attributeUpdaterAdd(
			"backgroundActivityFiltered", dvCfgType::LONG, &updateBackgroundActivityFiltered, noiseFilter.get(), false);
		moduleNode.attributeUpdaterAdd(
			"refractoryPeriodFiltered", dvCfgType::LONG, &updateRefractoryPeriodFiltered, noiseFilter.get(), false);

		moduleNode.addAttributeListener(noiseFilter.get(), &caerDVSNoiseFilterConfigCustom);
	}

	~DVSNoiseFilter() {
		// Remove listener, which can reference invalid memory in userData.
		moduleNode.removeAttributeListener(noiseFilter.get(), &caerDVSNoiseFilterConfigCustom);

		moduleNode.attributeUpdaterRemoveAll();
	}

	void advancedConfigUpdate() {
		caerFilterDVSNoiseConfigSet(
			noiseFilter.get(), CAER_FILTER_DVS_HOTPIXEL_TIME, U64T(config["hotPixelTime"].getValue<dvCfgType::INT>()));
		caerFilterDVSNoiseConfigSet(noiseFilter.get(), CAER_FILTER_DVS_HOTPIXEL_COUNT,
			U64T(config["hotPixelCount"].getValue<dvCfgType::INT>()));

		caerFilterDVSNoiseConfigSet(
			noiseFilter.get(), CAER_FILTER_DVS_HOTPIXEL_ENABLE, config["hotPixelEnable"].getValue<dvCfgType::BOOL>());

		caerFilterDVSNoiseConfigSet(noiseFilter.get(), CAER_FILTER_DVS_BACKGROUND_ACTIVITY_ENABLE,
			config["backgroundActivityEnable"].getValue<dvCfgType::BOOL>());
		caerFilterDVSNoiseConfigSet(noiseFilter.get(), CAER_FILTER_DVS_BACKGROUND_ACTIVITY_TWO_LEVELS,
			config["backgroundActivityTwoLevels"].getValue<dvCfgType::BOOL>());
		caerFilterDVSNoiseConfigSet(noiseFilter.get(), CAER_FILTER_DVS_BACKGROUND_ACTIVITY_CHECK_POLARITY,
			config["backgroundActivityCheckPolarity"].getValue<dvCfgType::BOOL>());
		caerFilterDVSNoiseConfigSet(noiseFilter.get(), CAER_FILTER_DVS_BACKGROUND_ACTIVITY_SUPPORT_MIN,
			U64T(config["backgroundActivitySupportMin"].getValue<dvCfgType::INT>()));
		caerFilterDVSNoiseConfigSet(noiseFilter.get(), CAER_FILTER_DVS_BACKGROUND_ACTIVITY_SUPPORT_MAX,
			U64T(config["backgroundActivitySupportMax"].getValue<dvCfgType::INT>()));
		caerFilterDVSNoiseConfigSet(noiseFilter.get(), CAER_FILTER_DVS_BACKGROUND_ACTIVITY_TIME,
			U64T(config["backgroundActivityTime"].getValue<dvCfgType::INT>()));

		caerFilterDVSNoiseConfigSet(noiseFilter.get(), CAER_FILTER_DVS_REFRACTORY_PERIOD_ENABLE,
			config["refractoryPeriodEnable"].getValue<dvCfgType::BOOL>());
		caerFilterDVSNoiseConfigSet(noiseFilter.get(), CAER_FILTER_DVS_REFRACTORY_PERIOD_TIME,
			U64T(config["refractoryPeriodTime"].getValue<dvCfgType::INT>()));

		caerFilterDVSNoiseConfigSet(
			noiseFilter.get(), CAER_FILTER_DVS_LOG_LEVEL, U64T(config["logLevel"].getValue<dvCfgType::INT>()));
	}

	void run() {
		// TODO: implement this.
		caerFilterDVSNoiseApply(noiseFilter.get(), nullptr);
	}
};

static union dvConfigAttributeValue updateHotPixelFiltered(
	void *userData, const char *key, enum dvConfigAttributeType type) {
	UNUSED_ARGUMENT(key);
	UNUSED_ARGUMENT(type);

	auto state = static_cast<caerFilterDVSNoise>(userData);
	union dvConfigAttributeValue statisticValue;
	statisticValue.ilong = 0;

	caerFilterDVSNoiseConfigGet(
		state, CAER_FILTER_DVS_HOTPIXEL_STATISTICS, reinterpret_cast<uint64_t *>(&statisticValue.ilong));

	return (statisticValue);
}

static union dvConfigAttributeValue updateBackgroundActivityFiltered(
	void *userData, const char *key, enum dvConfigAttributeType type) {
	UNUSED_ARGUMENT(key);
	UNUSED_ARGUMENT(type);

	auto state = static_cast<caerFilterDVSNoise>(userData);
	union dvConfigAttributeValue statisticValue;
	statisticValue.ilong = 0;

	caerFilterDVSNoiseConfigGet(
		state, CAER_FILTER_DVS_BACKGROUND_ACTIVITY_STATISTICS, reinterpret_cast<uint64_t *>(&statisticValue.ilong));

	return (statisticValue);
}

static union dvConfigAttributeValue updateRefractoryPeriodFiltered(
	void *userData, const char *key, enum dvConfigAttributeType type) {
	UNUSED_ARGUMENT(key);
	UNUSED_ARGUMENT(type);

	auto state = static_cast<caerFilterDVSNoise>(userData);
	union dvConfigAttributeValue statisticValue;
	statisticValue.ilong = 0;

	caerFilterDVSNoiseConfigGet(
		state, CAER_FILTER_DVS_REFRACTORY_PERIOD_STATISTICS, reinterpret_cast<uint64_t *>(&statisticValue.ilong));

	return (statisticValue);
}

static void caerDVSNoiseFilterConfigCustom(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(changeValue);

	auto state = static_cast<caerFilterDVSNoise>(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "hotPixelLearn")
		&& changeValue.boolean) {
		// Button-like configuration parameters need special
		// handling as only the change is delivered, so we have to listen for
		// it directly. The usual Config mechanism doesn't work, as Get()
		// would always return false.
		caerFilterDVSNoiseConfigSet(state, CAER_FILTER_DVS_HOTPIXEL_LEARN, true);

		// TODO: this should use AttributeUpdaters to keep track of the completion state.
		dvConfigNodeAttributeButtonReset(node, changeKey);
	}
}
