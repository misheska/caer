#include "mainloop.h"

static MainloopData *glMainloopDataPtr;

void dvMainloopSDKLibInit(MainloopData *setMainloopPtr) {
	glMainloopDataPtr = setMainloopPtr;
}

void dvMainloopDataNotifyIncrease(void *p) {
	UNUSED_ARGUMENT(p);

	glMainloopDataPtr->dataAvailable.fetch_add(1, std::memory_order_release);
}

void dvMainloopDataNotifyDecrease(void *p) {
	UNUSED_ARGUMENT(p);

	// No special memory order for decrease, because the acquire load to even start running
	// through a mainloop already synchronizes with the release store above.
	glMainloopDataPtr->dataAvailable.fetch_sub(1, std::memory_order_relaxed);
}

bool dvMainloopModuleExists(int16_t id) {
	return (glMainloopDataPtr->modules.count(id) == 1);
}

enum dvModuleType dvMainloopModuleGetType(int16_t id) {
	return (glMainloopDataPtr->modules.at(id).libraryInfo->type);
}

uint32_t dvMainloopModuleGetVersion(int16_t id) {
	return (glMainloopDataPtr->modules.at(id).libraryInfo->version);
}

enum dvModuleStatus dvMainloopModuleGetStatus(int16_t id) {
	return (glMainloopDataPtr->modules.at(id).runtimeData->moduleStatus);
}

dvConfigNode dvMainloopModuleGetConfigNode(int16_t id) {
	return (glMainloopDataPtr->modules.at(id).runtimeData->moduleNode);
}

size_t dvMainloopModuleGetInputDeps(int16_t id, int16_t **inputDepIds) {
	// Ensure is set to NULL for error return.
	// Support passing in NULL directly if not interested in result.
	if (inputDepIds != nullptr) {
		*inputDepIds = nullptr;
	}

	// Only makes sense to be called from PROCESSORs or OUTPUTs, as INPUTs
	// do not have inputs themselves.
	if (dvMainloopModuleGetType(id) == DV_MODULE_INPUT) {
		return (0);
	}

	std::vector<int16_t> inputModuleIds;
	inputModuleIds.reserve(glMainloopDataPtr->modules.at(id).inputDefinition.size());

	// Get all module IDs of inputs to this module (each present only once in
	// 'inputDefinition' of module), then sort them and return if so requested.
	for (auto &in : glMainloopDataPtr->modules.at(id).inputDefinition) {
		inputModuleIds.push_back(in.first);
	}

	std::sort(inputModuleIds.begin(), inputModuleIds.end());

	if (inputDepIds != nullptr && !inputModuleIds.empty()) {
		*inputDepIds = (int16_t *) malloc(inputModuleIds.size() * sizeof(int16_t));
		if (*inputDepIds == nullptr) {
			// Memory allocation failure, report as if nothing found.
			return (0);
		}

		memcpy(*inputDepIds, inputModuleIds.data(), inputModuleIds.size() * sizeof(int16_t));
	}

	return (inputModuleIds.size());
}

size_t dvMainloopModuleGetOutputRevDeps(int16_t id, int16_t **outputRevDepIds) {
	// Ensure is set to NULL for error return.
	// Support passing in NULL directly if not interested in result.
	if (outputRevDepIds != nullptr) {
		*outputRevDepIds = nullptr;
	}

	// Only makes sense to be called from INPUTs or PROCESSORs, as OUTPUTs
	// do not have outputs themselves.
	if (dvMainloopModuleGetType(id) == DV_MODULE_OUTPUT) {
		return (0);
	}

	std::vector<int16_t> outputModuleIds;

	// Get all IDs of modules that depend on outputs of this module.
	// Those are usually called reverse dependencies.
	// Look at all the streams that originate from this module ID,
	// if any exist take their users and then remove duplicates.
	for (const auto &st : glMainloopDataPtr->streams) {
		if (st.sourceId == id) {
			outputModuleIds.insert(outputModuleIds.end(), st.users.cbegin(), st.users.cend());
		}
	}

	vectorSortUnique(outputModuleIds);

	if (outputRevDepIds != nullptr && !outputModuleIds.empty()) {
		*outputRevDepIds = (int16_t *) malloc(outputModuleIds.size() * sizeof(int16_t));
		if (*outputRevDepIds == nullptr) {
			// Memory allocation failure, report as if nothing found.
			return (0);
		}

		memcpy(*outputRevDepIds, outputModuleIds.data(), outputModuleIds.size() * sizeof(int16_t));
	}

	return (outputModuleIds.size());
}

size_t dvMainloopModuleResetOutputRevDeps(int16_t id) {
	// Find and reset all reverse dependencies of a particular module.
	int16_t *outputRevDepIds;
	size_t numRevDeps = dvMainloopModuleGetOutputRevDeps(id, &outputRevDepIds);

	if (numRevDeps > 0) {
		for (size_t i = 0; i < numRevDeps; i++) {
			if (glMainloopDataPtr->modules.at(outputRevDepIds[i]).runtimeData->moduleStatus == DV_MODULE_RUNNING) {
				glMainloopDataPtr->modules.at(outputRevDepIds[i]).runtimeData->doReset.store(id);
			}
		}

		free(outputRevDepIds);
	}

	return (numRevDeps);
}

dvConfigNode dvMainloopModuleGetSourceNodeForInput(int16_t id, size_t inputNum) {
	int16_t *inputModules;
	size_t inputModulesNum = dvMainloopModuleGetInputDeps(id, &inputModules);

	if (inputNum >= inputModulesNum) {
		return (nullptr);
	}

	int16_t sourceId = inputModules[inputNum];

	free(inputModules);

	return (dvMainloopGetSourceNode(sourceId));
}

dvConfigNode dvMainloopModuleGetSourceInfoForInput(int16_t id, size_t inputNum) {
	int16_t *inputModules;
	size_t inputModulesNum = dvMainloopModuleGetInputDeps(id, &inputModules);

	if (inputNum >= inputModulesNum) {
		return (nullptr);
	}

	int16_t sourceId = inputModules[inputNum];

	free(inputModules);

	return (dvMainloopGetSourceInfo(sourceId));
}

static inline dvModuleData dvMainloopGetSourceData(int16_t sourceID) {
	// Sources must be INPUTs or PROCESSORs.
	if (dvMainloopModuleGetType(sourceID) == DV_MODULE_OUTPUT) {
		return (nullptr);
	}

	// Sources must actually produce some event stream.
	bool isSource = false;

	for (const auto &st : glMainloopDataPtr->streams) {
		if (st.sourceId == sourceID) {
			isSource = true;
			break;
		}
	}

	if (!isSource) {
		return (nullptr);
	}

	return (glMainloopDataPtr->modules.at(sourceID).runtimeData);
}

dvConfigNode dvMainloopGetSourceNode(int16_t sourceID) {
	dvModuleData moduleData = dvMainloopGetSourceData(sourceID);
	if (moduleData == nullptr) {
		return (nullptr);
	}

	return (moduleData->moduleNode);
}

void *dvMainloopGetSourceState(int16_t sourceID) {
	dvModuleData moduleData = dvMainloopGetSourceData(sourceID);
	if (moduleData == nullptr) {
		return (nullptr);
	}

	return (moduleData->moduleState);
}

dvConfigNode dvMainloopGetSourceInfo(int16_t sourceID) {
	dvModuleData moduleData = dvMainloopGetSourceData(sourceID);
	if (moduleData == nullptr) {
		return (nullptr);
	}

	// All sources should have a sub-node in ConfigTree called 'sourceInfo/',
	// while they are running only (so check running and existence).
	if (moduleData->moduleStatus == DV_MODULE_STOPPED) {
		return (nullptr);
	}

	if (!dvConfigNodeExistsRelativeNode(moduleData->moduleNode, "sourceInfo/")) {
		return (nullptr);
	}

	return (dvConfigNodeGetRelativeNode(moduleData->moduleNode, "sourceInfo/"));
}
