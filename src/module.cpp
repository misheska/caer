#include "module.h"

#include "log.hpp"

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <iterator>
#include <mutex>
#include <regex>
#include <thread>
#include <vector>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;
using logLevel   = libcaer::log::logLevel;

static void moduleShutdownListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void moduleLogLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

static struct {
	std::vector<boost::filesystem::path> modulePaths;
	std::recursive_mutex modulePathsMutex;
} glModuleData;

// Module-related definitions.
enum class ModuleStatus {
	DV_MODULE_STOPPED = 0,
	DV_MODULE_RUNNING = 1,
};

class ModuleOutput {
	dv::Types::Type type;
};

class ModuleInput {
	dv::Types::Type type;
	bool optional;
};

class Module {
	std::string name;
	dvModuleInfo info;
	ModuleLibrary library;
	ModuleStatus moduleStatus;
	std::atomic_bool running;
	logBlock logger;
	std::atomic_uint_fast32_t configUpdate;
	std::unordered_map<std::string, ModuleOutput> outputs;
	std::unordered_map<std::string, ModuleInput> inputs;
	dvModuleDataS data;

	Module(const std::string &_name, const std::string &_library) :
		name(_name),
		moduleStatus(ModuleStatus::DV_MODULE_STOPPED),
		running(false),
		configUpdate(0) {
		// Load library to get module functions.
		std::pair<ModuleLibrary, dvModuleInfo> mLoad;

		try {
			mLoad = dvModuleLoadLibrary(name);
		}
		catch (const std::exception &ex) {
			boost::format exMsg = boost::format("%s: module library load failed, error '%s'.") % name % ex.what();
			dvLog(logLevel::ERROR, exMsg);
			throw std::invalid_argument(exMsg.str().c_str());
		}

		// Set configuration node (so it's user accessible).
		dvCfg::Node moduleNode = dvCfg::GLOBAL.getNode("/mainloop/" + name + "/");

		data.moduleNode = static_cast<dvConfigNode>(moduleNode);

		// Ensure static configuration is created on each module initialization.
		StaticInit(moduleNode);

		// Per-module log level support.
		logger.logLevel.store(moduleNode.get<dvCfgType::INT>("logLevel"), std::memory_order_relaxed);
		moduleNode.addAttributeListener(&logger.logLevel, &moduleLogLevelListener);

		// Initialize shutdown controls.
		bool runModule = moduleNode.get<dvCfgType::BOOL>("autoStartup");

		moduleNode.put<dvCfgType::BOOL>("running", runModule);
		moduleNode.updateReadOnly<dvCfgType::BOOL>("isRunning", false);

		running.store(runModule, std::memory_order_relaxed);
		moduleNode.addAttributeListener(this, &moduleShutdownListener);

		std::atomic_thread_fence(std::memory_order_release);
	}

	~Module() {
		// Remove listener, which can reference invalid memory in userData.
		dvConfigNodeRemoveAttributeListener(moduleData->moduleNode, moduleData, &moduleShutdownListener);
		dvConfigNodeRemoveAttributeListener(moduleData->moduleNode, moduleData, &moduleLogLevelListener);
	}

	static void StaticInit(dv::Config::Node moduleNode) {
		// Per-module log level support. Initialize with global log level value.
		moduleNode.create<dvCfgType::INT>("logLevel", caerLogLevelGet(), {CAER_LOG_EMERGENCY, CAER_LOG_DEBUG},
			dvCfgFlags::NORMAL, "Module-specific log-level.");

		// Initialize shutdown controls. By default modules always run.
		// Allow for users to disable a module at start.
		moduleNode.create<dvCfgType::BOOL>("autoStartup", true, {}, dvCfgFlags::NORMAL,
			"Start this module when the mainloop starts and keep retrying if initialization fails.");

		moduleNode.create<dvCfgType::BOOL>(
			"running", false, {}, dvCfgFlags::NORMAL | dvCfgFlags::NO_EXPORT, "Module start/stop.");

		moduleNode.create<dvCfgType::BOOL>(
			"isRunning", false, {}, dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Module running state.");

		// Call module's configInit function to create default static config.

		if (mLoad.second->functions->moduleStaticInit != nullptr) {
			try {
				mLoad.second->functions->moduleStaticInit(static_cast<dvConfigNode>(moduleNode));
			}
			catch (const std::exception &ex) {
				boost::format exMsg = boost::format("moduleConfigInit() for '%s': %s") % moduleName % ex.what();
				libcaer::log::log(libcaer::log::logLevel::ERROR, moduleNode.getName().c_str(), exMsg.str().c_str());
			}
		}

		dvModuleUnloadLibrary(mLoad.first);

		// Each module can set priority attributes for UI display. By default let's show 'logLevel'.
		// Called last to allow for configInit() function to create a different default first.
		moduleNode.attributeModifierPriorityAttributes("logLevel");
	}
};

static inline void handleModuleInitFailure(dvCfg::Node moduleNode) {
	// Set running back to false on initialization failure.
	moduleNode.put<dvCfgType::BOOL>("running", false);

	// Schedule retry on next update-handler pass, if module should
	// automatically retry starting up and initializing.
	if (moduleNode.get<dvCfgType::BOOL>("autoStartup")) {
		moduleNode.attributeUpdaterAdd("running", dvCfgType::BOOL,
			[](void *, const char *, enum dvConfigAttributeType) {
				dvConfigAttributeValue val;
				val.boolean = true;
				return (val);
			},
			nullptr, true);
	}
}

void dvModuleSM(dvModuleFunctions moduleFunctions, dvModuleData moduleData, size_t memSize) {
	bool running = moduleData->running.load(std::memory_order_relaxed);
	dvCfg::Node moduleNode(moduleData->moduleNode);

	if (moduleData->moduleStatus == DV_MODULE_RUNNING && running) {
		if (moduleData->configUpdate.load(std::memory_order_relaxed) != 0) {
			moduleData->configUpdate.store(0);

			if (moduleFunctions->moduleConfig != nullptr) {
				// Call config function. 'configUpdate' variable reset is done above.
				try {
					moduleFunctions->moduleConfig(moduleData);
				}
				catch (const std::exception &ex) {
					libcaer::log::log(libcaer::log::logLevel::ERROR, moduleData->moduleSubSystemString,
						"moduleConfig(): '%s', disabling module.", ex.what());

					moduleNode.put<dvCfgType::BOOL>("running", false);
					return;
				}
			}
		}

		if (moduleFunctions->moduleRun != nullptr) {
			try {
				moduleFunctions->moduleRun(moduleData, in, out);
			}
			catch (const std::exception &ex) {
				libcaer::log::log(libcaer::log::logLevel::ERROR, moduleData->moduleSubSystemString,
					"moduleRun(): '%s', disabling module.", ex.what());

				moduleNode.put<dvCfgType::BOOL>("running", false);
				return;
			}
		}
	}
	else if (moduleData->moduleStatus == DV_MODULE_STOPPED && running) {
		// Check that all modules this module depends on are also running.
		int16_t *neededModules;
		size_t neededModulesSize = dvMainloopModuleGetInputDeps(moduleData->moduleID, &neededModules);

		if (neededModulesSize > 0) {
			for (size_t i = 0; i < neededModulesSize; i++) {
				if (dvMainloopModuleGetStatus(neededModules[i]) != DV_MODULE_RUNNING) {
					free(neededModules);

					handleModuleInitFailure(moduleNode);
					return;
				}
			}

			free(neededModules);
		}

		// Allocate memory for module state.
		if (memSize != 0) {
			moduleData->moduleState = calloc(1, memSize);
			if (moduleData->moduleState == nullptr) {
				handleModuleInitFailure(moduleNode);
				return;
			}
		}
		else {
			// memSize is zero, so moduleState must be nullptr.
			moduleData->moduleState = nullptr;
		}

		// Reset variables, as the following Init() is stronger than a reset
		// and implies a full configuration update. This avoids stale state
		// forcing an update and/or reset right away in the first run of
		// the module, which is unneeded and wasteful.
		moduleData->configUpdate.store(0);

		if (moduleFunctions->moduleInit != nullptr) {
			try {
				if (!moduleFunctions->moduleInit(moduleData)) {
					throw std::runtime_error("Failed to initialize module.");
				}
			}
			catch (const std::exception &ex) {
				libcaer::log::log(
					libcaer::log::logLevel::ERROR, moduleData->moduleSubSystemString, "moduleInit(): %s", ex.what());

				if (memSize != 0) {
					// Only deallocate if we were the original allocator.
					free(moduleData->moduleState);
				}
				moduleData->moduleState = nullptr;

				handleModuleInitFailure(moduleNode);
				return;
			}
		}

		moduleData->moduleStatus = DV_MODULE_RUNNING;
		moduleNode.updateReadOnly<dvCfgType::BOOL>("isRunning", true);

		// After starting successfully, try to enable dependent
		// modules if their 'autoStartup' is true. Else shutting down
		// an input would kill everything until mainloop restart.
		// This is consistent with initial mainloop start.
		int16_t *dependantModules;
		size_t dependantModulesSize = dvMainloopModuleGetOutputRevDeps(moduleData->moduleID, &dependantModules);

		if (dependantModulesSize > 0) {
			for (size_t i = 0; i < dependantModulesSize; i++) {
				dvCfg::Node moduleConfigNode(dvMainloopModuleGetConfigNode(dependantModules[i]));

				if (moduleConfigNode.get<dvCfgType::BOOL>("autoStartup")) {
					moduleNode.put<dvCfgType::BOOL>("running", true);
				}
			}

			free(dependantModules);
		}
	}
	else if (moduleData->moduleStatus == DV_MODULE_RUNNING && !running) {
		moduleData->moduleStatus = DV_MODULE_STOPPED;

		if (moduleFunctions->moduleExit != nullptr) {
			try {
				moduleFunctions->moduleExit(moduleData);
			}
			catch (const std::exception &ex) {
				libcaer::log::log(
					libcaer::log::logLevel::ERROR, moduleData->moduleSubSystemString, "moduleExit(): %s", ex.what());
			}
		}

		if (memSize != 0) {
			// Only deallocate if we were the original allocator.
			free(moduleData->moduleState);
		}
		moduleData->moduleState = nullptr;

		moduleNode.updateReadOnly<dvCfgType::BOOL>("isRunning", false);

		// Shutdown of module: ensure all modules depending on this
		// one also get stopped (running set to false).
		int16_t *dependantModules;
		size_t dependantModulesSize = dvMainloopModuleGetOutputRevDeps(moduleData->moduleID, &dependantModules);

		if (dependantModulesSize > 0) {
			for (size_t i = 0; i < dependantModulesSize; i++) {
				dvCfg::Node moduleConfigNode(dvMainloopModuleGetConfigNode(dependantModules[i]));

				moduleConfigNode.put<dvCfgType::BOOL>("running", false);
			}

			free(dependantModules);
		}
	}
}

static void moduleShutdownListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	auto data = static_cast<std::atomic_bool *>(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "running")) {
		data->store(changeValue.boolean);
	}
}

static void moduleLogLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	auto data = static_cast<std::atomic_int32_t *>(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "logLevel")) {
		data->store(changeValue.iint);
	}
}

std::pair<ModuleLibrary, dvModuleInfo> dvModuleLoadLibrary(const std::string &moduleName) {
	// For each module, we search if a path exists to load it from.
	// If yes, we do so. The various OS's shared library load mechanisms
	// will keep track of reference count if same module is loaded
	// multiple times.
	boost::filesystem::path modulePath;

	{
		std::lock_guard<std::recursive_mutex> lock(glModuleData.modulePathsMutex);

		for (const auto &p : glModuleData.modulePaths) {
			if (moduleName == p.stem().string()) {
				// Found a module with same name!
				modulePath = p;
			}
		}
	}

	if (modulePath.empty()) {
		boost::format exMsg = boost::format("No module library for '%s' found.") % moduleName;
		throw std::runtime_error(exMsg.str());
	}

#if BOOST_HAS_DLL_LOAD
	ModuleLibrary moduleLibrary;
	try {
		moduleLibrary.load(modulePath.c_str(), boost::dll::load_mode::rtld_now);
	}
	catch (const std::exception &ex) {
		// Failed to load shared library!
		boost::format exMsg
			= boost::format("Failed to load library '%s', error: '%s'.") % modulePath.string() % ex.what();
		throw std::runtime_error(exMsg.str());
	}

	dvModuleInfo (*getInfo)(void);
	try {
		getInfo = moduleLibrary.get<dvModuleInfo(void)>("dvModuleGetInfo");
	}
	catch (const std::exception &ex) {
		// Failed to find symbol in shared library!
		dvModuleUnloadLibrary(moduleLibrary);
		boost::format exMsg
			= boost::format("Failed to find symbol in library '%s', error: '%s'.") % modulePath.string() % ex.what();
		throw std::runtime_error(exMsg.str());
	}
#else
	void *moduleLibrary = dlopen(modulePath.c_str(), RTLD_NOW);
	if (moduleLibrary == nullptr) {
		// Failed to load shared library!
		boost::format exMsg
			= boost::format("Failed to load library '%s', error: '%s'.") % modulePath.string() % dlerror();
		throw std::runtime_error(exMsg.str());
	}

	dvModuleInfo (*getInfo)(void) = (dvModuleInfo(*)(void)) dlsym(moduleLibrary, "dvModuleGetInfo");
	if (getInfo == nullptr) {
		// Failed to find symbol in shared library!
		dvModuleUnloadLibrary(moduleLibrary);
		boost::format exMsg
			= boost::format("Failed to find symbol in library '%s', error: '%s'.") % modulePath.string() % dlerror();
		throw std::runtime_error(exMsg.str());
	}
#endif

	dvModuleInfo info = (*getInfo)();
	if (info == nullptr) {
		dvModuleUnloadLibrary(moduleLibrary);
		boost::format exMsg = boost::format("Failed to get info from library '%s'.") % modulePath.string();
		throw std::runtime_error(exMsg.str());
	}

	return (std::pair<ModuleLibrary, dvModuleInfo>(moduleLibrary, info));
}

// Small helper to unload libraries on error.
void dvModuleUnloadLibrary(ModuleLibrary &moduleLibrary) {
#if BOOST_HAS_DLL_LOAD
	moduleLibrary.unload();
#else
	dlclose(moduleLibrary);
#endif
}

static void checkInputOutputStreamDefinitions(dvModuleInfo info) {
	if (info->type == DV_MODULE_INPUT) {
		if (info->inputStreams != nullptr || info->inputStreamsSize != 0 || info->outputStreams == nullptr
			|| info->outputStreamsSize == 0) {
			throw std::domain_error("Wrong I/O event stream definitions for type INPUT.");
		}
	}
	else if (info->type == DV_MODULE_OUTPUT) {
		if (info->inputStreams == nullptr || info->inputStreamsSize == 0 || info->outputStreams != nullptr
			|| info->outputStreamsSize != 0) {
			throw std::domain_error("Wrong I/O event stream definitions for type OUTPUT.");
		}

		// Also ensure that all input streams of an output module are marked read-only.
		bool readOnlyError = false;

		for (size_t i = 0; i < info->inputStreamsSize; i++) {
			if (!info->inputStreams[i].readOnly) {
				readOnlyError = true;
				break;
			}
		}

		if (readOnlyError) {
			throw std::domain_error("Input event streams not marked read-only for type OUTPUT.");
		}
	}
	else {
		// DV_MODULE_PROCESSOR
		if (info->inputStreams == nullptr || info->inputStreamsSize == 0) {
			throw std::domain_error("Wrong I/O event stream definitions for type PROCESSOR.");
		}

		// If no output streams are defined, then at least one input event
		// stream must not be readOnly, so that there is modified data to output.
		if (info->outputStreams == nullptr || info->outputStreamsSize == 0) {
			bool readOnlyError = true;

			for (size_t i = 0; i < info->inputStreamsSize; i++) {
				if (!info->inputStreams[i].readOnly) {
					readOnlyError = false;
					break;
				}
			}

			if (readOnlyError) {
				throw std::domain_error(
					"No output streams and all input streams are marked read-only for type PROCESSOR.");
			}
		}
	}
}

/**
 * Type must be either -1 or well defined (0-INT16_MAX).
 * Number must be either -1 or well defined (1-INT16_MAX). Zero not allowed.
 * The event stream array must be ordered by ascending type ID.
 * For each type, only one definition can exist.
 * If type is -1 (any), then number must also be -1; having a defined
 * number in this case makes no sense (N of any type???), a special exception
 * is made for the number 1 (1 of any type) with inputs, which can be useful.
 * Also this must then be the only definition.
 * If number is -1, then either the type is also -1 and this is the
 * only event stream definition (same as rule above), OR the type is well
 * defined and this is the only event stream definition for that type.
 */
static void checkInputStreamDefinitions(caerEventStreamIn inputStreams, size_t inputStreamsSize) {
	for (size_t i = 0; i < inputStreamsSize; i++) {
		// Check type range.
		if (inputStreams[i].type < -1) {
			throw std::domain_error("Input stream has invalid type value.");
		}

		// Check number range.
		if (inputStreams[i].number < -1 || inputStreams[i].number == 0) {
			throw std::domain_error("Input stream has invalid number value.");
		}

		// Check sorted array and only one definition per type; the two
		// requirements together mean strict monotonicity for types.
		if (i > 0 && inputStreams[i - 1].type >= inputStreams[i].type) {
			throw std::domain_error("Input stream has invalid order of declaration or duplicates.");
		}

		// Check that any type is always together with any number or 1, and the
		// only definition present in that case.
		if (inputStreams[i].type == -1
			&& ((inputStreams[i].number != -1 && inputStreams[i].number != 1) || inputStreamsSize != 1)) {
			throw std::domain_error("Input stream has invalid any declaration.");
		}
	}
}

/**
 * Type must be either -1 or well defined (0-INT16_MAX).
 * The event stream array must be ordered by ascending type ID.
 * For each type, only one definition can exist.
 * If type is -1 (any), then this must then be the only definition.
 */
static void checkOutputStreamDefinitions(caerEventStreamOut outputStreams, size_t outputStreamsSize) {
	// If type is any, must be the only definition.
	if (outputStreamsSize == 1 && outputStreams[0].type == -1) {
		return;
	}

	for (size_t i = 0; i < outputStreamsSize; i++) {
		// Check type range.
		if (outputStreams[i].type < 0) {
			throw std::domain_error("Output stream has invalid type value.");
		}

		// Check sorted array and only one definition per type; the two
		// requirements together mean strict monotonicity for types.
		if (i > 0 && outputStreams[i - 1].type >= outputStreams[i].type) {
			throw std::domain_error("Output stream has invalid order of declaration or duplicates.");
		}
	}
}

void dvUpdateModulesInformation() {
	std::lock_guard<std::recursive_mutex> lock(glModuleData.modulePathsMutex);

	auto modulesNode = dvCfg::GLOBAL.getNode("/system/modules/");

	// Clear out modules information.
	modulesNode.clearSubTree(false);
	modulesNode.removeSubTree();
	glModuleData.modulePaths.clear();

	// Search for available modules. Will be loaded as needed later.
	const std::string modulesSearchPath = modulesNode.get<dvCfgType::STRING>("modulesSearchPath");

	// Split on '|'.
	boost::tokenizer<boost::char_separator<char>> searchPaths(modulesSearchPath, boost::char_separator<char>("|"));

	// Search is recursive for binary shared libraries.
	const std::regex moduleRegex("\\w+\\.(so|dll|dylib)");

	for (const auto &sPath : searchPaths) {
		if (!boost::filesystem::exists(sPath)) {
			continue;
		}

		std::for_each(boost::filesystem::recursive_directory_iterator(sPath),
			boost::filesystem::recursive_directory_iterator(),
			[&moduleRegex](const boost::filesystem::directory_entry &e) {
				if (boost::filesystem::exists(e.path()) && boost::filesystem::is_regular_file(e.path())
					&& std::regex_match(e.path().filename().string(), moduleRegex)) {
					glModuleData.modulePaths.push_back(e.path());
				}
			});
	}

	// Sort and unique.
	vectorSortUnique(glModuleData.modulePaths);

	// No modules, cannot start!
	if (glModuleData.modulePaths.empty()) {
		boost::format exMsg = boost::format("Failed to find any modules on path(s) '%s'.") % modulesSearchPath;
		throw std::runtime_error(exMsg.str());
	}

	// Generate nodes for each module, with their in/out information as attributes.
	// This also checks basic validity of the module's information.
	auto iter = std::begin(glModuleData.modulePaths);

	while (iter != std::end(glModuleData.modulePaths)) {
		std::string moduleName = iter->stem().string();

		// Load library.
		std::pair<ModuleLibrary, dvModuleInfo> mLoad;

		try {
			mLoad = dvModuleLoadLibrary(moduleName);
		}
		catch (const std::exception &ex) {
			boost::format exMsg = boost::format("Module '%s': %s") % moduleName % ex.what();
			libcaer::log::log(libcaer::log::logLevel::ERROR, "Module", exMsg.str().c_str());

			iter = glModuleData.modulePaths.erase(iter);
			continue;
		}

		try {
			// Check that the modules respect the basic I/O definition requirements.
			checkInputOutputStreamDefinitions(mLoad.second);

			// Check I/O event stream definitions for correctness.
			if (mLoad.second->inputStreams != nullptr) {
				checkInputStreamDefinitions(mLoad.second->inputStreams, mLoad.second->inputStreamsSize);
			}

			if (mLoad.second->outputStreams != nullptr) {
				checkOutputStreamDefinitions(mLoad.second->outputStreams, mLoad.second->outputStreamsSize);
			}
		}
		catch (const std::exception &ex) {
			boost::format exMsg = boost::format("Module '%s': %s") % moduleName % ex.what();
			libcaer::log::log(libcaer::log::logLevel::ERROR, "Module", exMsg.str().c_str());

			dvModuleUnloadLibrary(mLoad.first);

			iter = glModuleData.modulePaths.erase(iter);
			continue;
		}

		// Get ConfigTree node under /system/modules/.
		auto moduleNode = modulesNode.getRelativeNode(moduleName + "/");

		// Parse dvModuleInfo into ConfigTree.
		moduleNode.create<dvCfgType::INT>("version", I32T(mLoad.second->version), {0, INT32_MAX},
			dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Module version.");
		moduleNode.create<dvCfgType::STRING>("description", mLoad.second->description, {1, 8192},
			dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Module description.");
		moduleNode.create<dvCfgType::STRING>("type", dvModuleTypeToString(mLoad.second->type), {1, 64},
			dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Module type.");
		moduleNode.create<dvCfgType::STRING>(
			"path", iter->string(), {1, PATH_MAX}, dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Module file path.");

		if (mLoad.second->inputStreamsSize > 0) {
			auto inputStreamsNode = moduleNode.getRelativeNode("inputStreams/");

			inputStreamsNode.create<dvCfgType::INT>("size", I32T(mLoad.second->inputStreamsSize), {1, INT16_MAX},
				dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Number of input streams.");

			for (size_t i = 0; i < mLoad.second->inputStreamsSize; i++) {
				auto inputStreamNode          = inputStreamsNode.getRelativeNode(std::to_string(i) + "/");
				caerEventStreamIn inputStream = &mLoad.second->inputStreams[i];

				inputStreamNode.create<dvCfgType::INT>("type", inputStream->type, {I16T(-1), I16T(INT16_MAX)},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Input event type (-1 for any type).");
				inputStreamNode.create<dvCfgType::INT>("number", inputStream->number, {I16T(-1), I16T(INT16_MAX)},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT,
					"Number of inputs of this type (-1 for any number).");
				inputStreamNode.create<dvCfgType::BOOL>("readOnly", inputStream->readOnly, {},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Whether this input is modified or not.");
			}
		}

		if (mLoad.second->outputStreamsSize > 0) {
			auto outputStreamsNode = moduleNode.getRelativeNode("outputStreams/");

			outputStreamsNode.create<dvCfgType::INT>("size", I32T(mLoad.second->outputStreamsSize), {1, INT16_MAX},
				dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Number of output streams.");

			for (size_t i = 0; i < mLoad.second->outputStreamsSize; i++) {
				auto outputStreamNode           = outputStreamsNode.getRelativeNode(std::to_string(i) + "/");
				caerEventStreamOut outputStream = &mLoad.second->outputStreams[i];

				outputStreamNode.create<dvCfgType::INT>("type", outputStream->type, {I16T(-1), I16T(INT16_MAX)},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT,
					"Output event type (-1 for undefined output determined at runtime).");
			}
		}

		// Done, unload library.
		dvModuleUnloadLibrary(mLoad.first);

		iter++;
	}
}
