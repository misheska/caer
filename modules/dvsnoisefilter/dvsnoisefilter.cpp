#define DV_API_OPENCV_SUPPORT 0
#include "dv-sdk/data/event.hpp"
#include "dv-sdk/module.hpp"

#include <array>
#include <vector>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

struct pixel_with_count {
	int16_t x;
	int16_t y;
	int32_t count;
};

#define GET_TS(X) ((X) >> 1)
#define GET_POL(X) ((X) &0x01)
#define SET_TSPOL(TS, POL) (((TS) << 1) | ((POL) &0x01))

class DVSNoiseFilter : public dv::ModuleBase {
private:
	// Hot Pixel filter (learning).
	bool hotPixelLearningStarted;
	int64_t hotPixelLearningStartTime;
	std::vector<int32_t> hotPixelLearningMap;
	// Hot Pixel filter (filtering).
	std::vector<pixel_with_count> hotPixelArray;
	int64_t hotPixelStatOn;
	int64_t hotPixelStatOff;
	// Background Activity filter.
	std::array<size_t, 8> supportPixelIndexes;
	int64_t backgroundActivityStatOn;
	int64_t backgroundActivityStatOff;
	// Refractory Period filter.
	int64_t refractoryPeriodStatOn;
	int64_t refractoryPeriodStatOff;
	// Maps and their sizes.
	int16_t sizeX;
	int16_t sizeY;
	std::vector<int64_t> timestampsMap;

public:
	static void addInputs(dv::InputDefinitionList &in) {
		in.addEventInput("events");
	}

	static void addOutputs(dv::OutputDefinitionList &out) {
		out.addEventOutput("events");
	}

	static const char *getDescription() {
		return ("Filters out noise from DVS change (polarity) events.");
	}

	static void getConfigOptions(dv::RuntimeConfig &config) {
		config.add("hotPixelLearn",
			dv::ConfigOption::boolOption(
				"Learn the position of current hot (abnormally active) pixels, so they can be filtered out.", false,
				dv::ButtonMode::EXECUTE));
		config.add(
			"hotPixelTime", dv::ConfigOption::intOption(
								"Time in µs to accumulate events for learning new hot pixels.", 1000000, 0, 30000000));
		config.add(
			"hotPixelCount", dv::ConfigOption::intOption(
								 "Number of events needed in a learning time period for a pixel to be considered hot.",
								 10000, 0, 10000000));
		config.add("hotPixelFilteredOn",
			dv::ConfigOption::statisticOption("Number of ON events filtered out by the hot pixel filter."));
		config.add("hotPixelFilteredOff",
			dv::ConfigOption::statisticOption("Number of OFF events filtered out by the hot pixel filter."));

		config.add("hotPixelEnable", dv::ConfigOption::boolOption("Enable the hot pixel filter.", false));

		config.add(
			"backgroundActivityEnable", dv::ConfigOption::boolOption("Enable the background activity filter.", true));
		config.add("backgroundActivityTwoLevels",
			dv::ConfigOption::boolOption("Use two-level background activity filtering."));
		config.add("backgroundActivityCheckPolarity",
			dv::ConfigOption::boolOption("Consider polarity when filtering background activity."));
		config.add("backgroundActivitySupportMin",
			dv::ConfigOption::intOption(
				"Minimum number of direct neighbor pixels that must support this pixel for it to be valid.", 1, 1, 8));
		config.add("backgroundActivitySupportMax",
			dv::ConfigOption::intOption(
				"Maximum number of direct neighbor pixels that can support this pixel for it to be valid.", 8, 1, 8));
		config.add("backgroundActivityTime",
			dv::ConfigOption::intOption(
				"Maximum time difference in µs for events to be considered correlated and not be filtered out.", 2000,
				0, 10000000));
		config.add("backgroundActivityFilteredOn",
			dv::ConfigOption::statisticOption("Number of ON events filtered out by the background activity filter."));
		config.add("backgroundActivityFilteredOff",
			dv::ConfigOption::statisticOption("Number of OFF events filtered out by the background activity filter."));

		config.add(
			"refractoryPeriodEnable", dv::ConfigOption::boolOption("Enable the refractory period filter.", true));
		config.add("refractoryPeriodTime",
			dv::ConfigOption::intOption("Minimum time between events to not be filtered out.", 100, 0, 10000000));
		config.add("refractoryPeriodFilteredOn",
			dv::ConfigOption::statisticOption("Number of ON events filtered out by the refractory period filter."));
		config.add("refractoryPeriodFilteredOff",
			dv::ConfigOption::statisticOption("Number of OFF events filtered out by the refractory period filter."));
	}

	DVSNoiseFilter() :
		hotPixelLearningStarted(false),
		hotPixelLearningStartTime(0),
		hotPixelStatOn(0),
		hotPixelStatOff(0),
		backgroundActivityStatOn(0),
		backgroundActivityStatOff(0),
		refractoryPeriodStatOn(0),
		refractoryPeriodStatOff(0) {
		auto eventInput = inputs.getEventInput("events");

		sizeX = static_cast<int16_t>(eventInput.sizeX());
		sizeY = static_cast<int16_t>(eventInput.sizeY());

		timestampsMap.resize(static_cast<size_t>(sizeX * sizeY));

		// Populate event output info node, keep same as input info node.
		outputs.getEventOutput("events").setup(inputs.getEventInput("events"));
	}

	void run() override {
		auto evt_in  = inputs.getEventInput("events").events();
		auto evt_out = outputs.getEventOutput("events").events();

		bool hotPixelEnabled           = config.get<dvCfgType::BOOL>("hotPixelEnable");
		bool refractoryPeriodEnabled   = config.get<dvCfgType::BOOL>("refractoryPeriodEnable");
		bool backgroundActivityEnabled = config.get<dvCfgType::BOOL>("backgroundActivityEnable");

		int32_t hotPixelTime                 = config.get<dvCfgType::INT>("hotPixelTime");
		int32_t refractoryPeriodTime         = config.get<dvCfgType::INT>("refractoryPeriodTime");
		int32_t backgroundActivitySupportMin = config.get<dvCfgType::INT>("backgroundActivitySupportMin");
		int32_t backgroundActivitySupportMax = config.get<dvCfgType::INT>("backgroundActivitySupportMax");
		int32_t backgroundActivityTime       = config.get<dvCfgType::INT>("backgroundActivityTime");
		bool backgroundActivityCheckPolarity = config.get<dvCfgType::BOOL>("backgroundActivityCheckPolarity");
		bool backgroundActivityTwoLevels     = config.get<dvCfgType::BOOL>("backgroundActivityTwoLevels");

		// Hot Pixel learning: initialize and store packet-level timestamp.
		if (config.get<dvCfgType::BOOL>("hotPixelLearn") && !hotPixelLearningStarted) {
			// Initialize hot pixel learning.
			hotPixelLearningMap.resize(static_cast<size_t>(sizeX * sizeY));

			hotPixelLearningStarted = true;

			// Store start timestamp.
			hotPixelLearningStartTime = evt_in[0].timestamp();

			log.debug << "HotPixel Learning: started on ts=" << hotPixelLearningStartTime << "." << dv::logEnd;
		}

		for (const auto &evt : evt_in) {
			size_t pixelIndex = static_cast<size_t>((evt.y() * sizeX) + evt.x()); // Target pixel.

			// Hot Pixel learning: determine which pixels are abnormally active,
			// by counting how many times they spike in a given time period. The
			// ones above a given threshold will be considered "hot".
			// This is done first, so that other filters (including the Hot Pixel
			// filter itself) don't influence the learning operation.
			if (hotPixelLearningStarted) {
				hotPixelLearningMap[pixelIndex]++;

				if (evt.timestamp() > (hotPixelLearningStartTime + hotPixelTime)) {
					// Enough time has passed, we can proceed with data evaluation.
					hotPixelGenerateArray();

					// Done, reset and notify end of learning.
					hotPixelLearningMap.clear();
					hotPixelLearningMap.shrink_to_fit();

					hotPixelLearningStarted = false;

					config.set<dvCfgType::BOOL>("hotPixelLearn", false);

					log.debug << "HotPixel Learning: completed on ts=" << evt.timestamp() << "." << dv::logEnd;
				}
			}

			// Hot Pixel filter: filter out abnormally active pixels by their address.
			if (hotPixelEnabled) {
				bool filteredOut = std::any_of(hotPixelArray.cbegin(), hotPixelArray.cend(),
					[&evt](const auto &px) { return ((evt.x() == px.x) && (evt.y() == px.y)); });

				if (filteredOut) {
					if (evt.polarity()) {
						hotPixelStatOn++;
					}
					else {
						hotPixelStatOff++;
					}

					// Go to next event, don't execute other filters and don't
					// update timestamps map. Hot pixels don't provide any useful
					// timing information, as they are repeating noise.
					continue;
				}
			}

			// Refractory Period filter.
			// Execute before BAFilter, as this is a much simpler check, so if we
			// can we try to eliminate the event early in a less costly manner.
			if (refractoryPeriodEnabled) {
				if ((evt.timestamp() - GET_TS(timestampsMap[pixelIndex])) < refractoryPeriodTime) {
					if (evt.polarity()) {
						refractoryPeriodStatOn++;
					}
					else {
						refractoryPeriodStatOff++;
					}

					// Invalid, jump.
					goto WriteTimestamp;
				}
			}

			if (backgroundActivityEnabled) {
				size_t supportPixelNum
					= doBackgroundActivityLookup(evt.x(), evt.y(), pixelIndex, evt.timestamp(), evt.polarity(),
						supportPixelIndexes.data(), backgroundActivityTime, backgroundActivityCheckPolarity);

				bool filteredOut = true;

				if ((supportPixelNum >= static_cast<size_t>(backgroundActivitySupportMin))
					&& (supportPixelNum <= static_cast<size_t>(backgroundActivitySupportMax))) {
					if (backgroundActivityTwoLevels) {
						// Do the check again for all previously discovered supporting pixels.
						for (size_t i = 0; i < supportPixelNum; i++) {
							size_t supportPixelIndex = supportPixelIndexes[i];
							int16_t supportPixelX
								= static_cast<int16_t>(supportPixelIndex % static_cast<size_t>(sizeX));
							int16_t supportPixelY
								= static_cast<int16_t>(supportPixelIndex / static_cast<size_t>(sizeX));

							if (doBackgroundActivityLookup(supportPixelX, supportPixelY, supportPixelIndex,
									evt.timestamp(), evt.polarity(), nullptr, backgroundActivityTime,
									backgroundActivityCheckPolarity)
								> 0) {
								filteredOut = false;
								break;
							}
						}
					}
					else {
						filteredOut = false;
					}
				}

				if (filteredOut) {
					if (evt.polarity()) {
						backgroundActivityStatOn++;
					}
					else {
						backgroundActivityStatOff++;
					}

					// Invalid, jump.
					goto WriteTimestamp;
				}
			}

			// Valid event.
			evt_out.emplace_back(evt);

		WriteTimestamp:
			// Update pixel timestamp (one write). Always update so filters are
			// ready at enable-time right away.
			timestampsMap[pixelIndex] = SET_TSPOL(evt.timestamp(), evt.polarity());
		}

		evt_out.commit();

		// Update statistics.
		config.set<dvCfgType::LONG>("hotPixelFilteredOn", hotPixelStatOn);
		config.set<dvCfgType::LONG>("hotPixelFilteredOff", hotPixelStatOff);
		config.set<dvCfgType::LONG>("backgroundActivityFilteredOn", backgroundActivityStatOn);
		config.set<dvCfgType::LONG>("backgroundActivityFilteredOff", backgroundActivityStatOff);
		config.set<dvCfgType::LONG>("refractoryPeriodFilteredOn", refractoryPeriodStatOn);
		config.set<dvCfgType::LONG>("refractoryPeriodFilteredOff", refractoryPeriodStatOff);
	}

	inline size_t doBackgroundActivityLookup(int16_t x, int16_t y, size_t pixelIndex, int64_t timestamp, bool polarity,
		size_t *supportIndexes, int32_t backgroundActivityTime, bool backgroundActivityCheckPolarity) {
		// Compute map limits.
		bool notBorderLeft  = (x != 0);
		bool notBorderDown  = (y != (sizeY - 1));
		bool notBorderRight = (x != (sizeX - 1));
		bool notBorderUp    = (y != 0);

		// Background Activity filter: if difference between current timestamp
		// and stored neighbor timestamp is smaller than given time limit, it
		// means the event is supported by a neighbor and thus valid. If it is
		// bigger, then the event is not supported, and we need to check the
		// next neighbor. If all are bigger, the event is invalid.
		size_t result = 0;

		{
			if (notBorderLeft) {
				pixelIndex--;

				if ((timestamp - GET_TS(timestampsMap[pixelIndex])) < backgroundActivityTime) {
					if (!backgroundActivityCheckPolarity || polarity == GET_POL(timestampsMap[pixelIndex])) {
						if (supportIndexes != nullptr) {
							supportIndexes[result] = pixelIndex;
						}
						result++;
					}
				}

				pixelIndex++;
			}

			if (notBorderRight) {
				pixelIndex++;

				if ((timestamp - GET_TS(timestampsMap[pixelIndex])) < backgroundActivityTime) {
					if (!backgroundActivityCheckPolarity || polarity == GET_POL(timestampsMap[pixelIndex])) {
						if (supportIndexes != nullptr) {
							supportIndexes[result] = pixelIndex;
						}
						result++;
					}
				}

				pixelIndex--;
			}
		}

		if (notBorderUp) {
			pixelIndex -= static_cast<size_t>(sizeX); // Previous row.

			if ((timestamp - GET_TS(timestampsMap[pixelIndex])) < backgroundActivityTime) {
				if (!backgroundActivityCheckPolarity || polarity == GET_POL(timestampsMap[pixelIndex])) {
					if (supportIndexes != nullptr) {
						supportIndexes[result] = pixelIndex;
					}
					result++;
				}
			}

			if (notBorderLeft) {
				pixelIndex--;

				if ((timestamp - GET_TS(timestampsMap[pixelIndex])) < backgroundActivityTime) {
					if (!backgroundActivityCheckPolarity || polarity == GET_POL(timestampsMap[pixelIndex])) {
						if (supportIndexes != nullptr) {
							supportIndexes[result] = pixelIndex;
						}
						result++;
					}
				}

				pixelIndex++;
			}

			if (notBorderRight) {
				pixelIndex++;

				if ((timestamp - GET_TS(timestampsMap[pixelIndex])) < backgroundActivityTime) {
					if (!backgroundActivityCheckPolarity || polarity == GET_POL(timestampsMap[pixelIndex])) {
						if (supportIndexes != nullptr) {
							supportIndexes[result] = pixelIndex;
						}
						result++;
					}
				}

				pixelIndex--;
			}

			pixelIndex += static_cast<size_t>(sizeX); // Back to middle row.
		}

		if (notBorderDown) {
			pixelIndex += static_cast<size_t>(sizeX); // Next row.

			if ((timestamp - GET_TS(timestampsMap[pixelIndex])) < backgroundActivityTime) {
				if (!backgroundActivityCheckPolarity || polarity == GET_POL(timestampsMap[pixelIndex])) {
					if (supportIndexes != nullptr) {
						supportIndexes[result] = pixelIndex;
					}
					result++;
				}
			}

			if (notBorderLeft) {
				pixelIndex--;

				if ((timestamp - GET_TS(timestampsMap[pixelIndex])) < backgroundActivityTime) {
					if (!backgroundActivityCheckPolarity || polarity == GET_POL(timestampsMap[pixelIndex])) {
						if (supportIndexes != nullptr) {
							supportIndexes[result] = pixelIndex;
						}
						result++;
					}
				}

				pixelIndex++;
			}

			if (notBorderRight) {
				pixelIndex++;

				if ((timestamp - GET_TS(timestampsMap[pixelIndex])) < backgroundActivityTime) {
					if (!backgroundActivityCheckPolarity || polarity == GET_POL(timestampsMap[pixelIndex])) {
						if (supportIndexes != nullptr) {
							supportIndexes[result] = pixelIndex;
						}
						result++;
					}
				}
			}
		}

		return (result);
	}

	void hotPixelGenerateArray() {
		// Clear out current values.
		hotPixelArray.clear();

		int32_t hotPixelCount = config.get<dvCfgType::INT>("hotPixelCount");

		// Find abnormally active pixels.
		for (size_t i = 0; i < hotPixelLearningMap.size(); i++) {
			if (hotPixelLearningMap[i] >= hotPixelCount) {
				pixel_with_count elem;

				elem.x     = static_cast<int16_t>(i % static_cast<size_t>(sizeX));
				elem.y     = static_cast<int16_t>(i / static_cast<size_t>(sizeX));
				elem.count = hotPixelLearningMap[i];

				hotPixelArray.push_back(elem);
			}
		}

		hotPixelArray.shrink_to_fit();

		// Sort in descending order by count.
		std::sort(hotPixelArray.begin(), hotPixelArray.end(),
			[](const pixel_with_count &a, const pixel_with_count &b) { return (a.count > b.count); });

		// Print list of hot pixels for debugging.
		for (size_t i = 0; i < hotPixelArray.size(); i++) {
			log.debug.format("HotPixel %zu: X=%" PRIi16 ", Y=%" PRIi16 ", count=%" PRIi32 ".", i, hotPixelArray[i].x,
				hotPixelArray[i].y, hotPixelArray[i].count);
		}
	}
};

registerModuleClass(DVSNoiseFilter)
