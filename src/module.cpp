#include "module.h"

#include "dv-sdk/cross/portable_io.h"

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

static struct {
	std::vector<boost::filesystem::path> modulePaths;
	std::recursive_mutex modulePathsMutex;
} glModuleData;

Module::Module(const std::string &_name, const std::string &_library) :
	name(_name),
	moduleStatus(ModuleStatus::STOPPED),
	running(false),
	configUpdate(0) {
	// Load library to get module functions.
	std::pair<ModuleLibrary, dvModuleInfo> mLoad;

	try {
		std::tie(library, info) = dvModuleLoadLibrary(_library);
	}
	catch (const std::exception &ex) {
		boost::format exMsg = boost::format("%s: module library load failed, error '%s'.") % name % ex.what();
		dv::Log(dv::logLevel::ERROR, exMsg);
		throw std::invalid_argument(exMsg.str().c_str());
	}

	// Set configuration node (so it's user accessible).
	dvCfg::Node moduleNode = dvCfg::GLOBAL.getNode("/mainloop/" + name + "/");

	data.moduleNode = static_cast<dvConfigNode>(moduleNode);

	// State allocated later by init().
	data.moduleState = nullptr;

	// Initialize logging related functionality.
	LoggingInit(moduleNode);

	// Initialize running related functionality.
	RunningInit(moduleNode);

	// Ensure static configuration is created on each module initialization.
	StaticInit(moduleNode);
}

Module::~Module() {
	// Remove listener, which can reference invalid memory in userData.
	dvConfigNodeRemoveAllAttributeListeners(data.moduleNode);

	dvModuleUnloadLibrary(library);
}

void Module::LoggingInit(dvCfg::Node &moduleNode) {
	// Per-module custom log string prefix.
	logger.logPrefix = name;

	// Per-module log level support. Initialize with global log level value.
	moduleNode.create<dvCfgType::INT>("logLevel", caerLogLevelGet(), {CAER_LOG_EMERGENCY, CAER_LOG_DEBUG},
		dvCfgFlags::NORMAL, "Module-specific log-level.");

	logger.logLevel.store(moduleNode.get<dvCfgType::INT>("logLevel"));
	moduleNode.addAttributeListener(&logger.logLevel, &moduleLogLevelListener);
}

void Module::RunningInit(dvCfg::Node &moduleNode) {
	// Initialize shutdown controls. By default modules always run.
	// Allow for users to disable a module at start.
	moduleNode.create<dvCfgType::BOOL>("autoStartup", true, {}, dvCfgFlags::NORMAL,
		"Start this module when the mainloop starts and keep retrying if initialization fails.");

	moduleNode.create<dvCfgType::BOOL>(
		"running", false, {}, dvCfgFlags::NORMAL | dvCfgFlags::NO_EXPORT, "Module start/stop.");

	moduleNode.create<dvCfgType::BOOL>(
		"isRunning", false, {}, dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Module running state.");

	bool runModule = moduleNode.get<dvCfgType::BOOL>("autoStartup");

	moduleNode.put<dvCfgType::BOOL>("running", runModule);
	moduleNode.updateReadOnly<dvCfgType::BOOL>("isRunning", false);

	running.store(runModule);
	moduleNode.addAttributeListener(&running, &moduleShutdownListener);
}

void Module::StaticInit(dvCfg::Node &moduleNode) {
	moduleNode.addAttributeListener(&configUpdate, &moduleConfigUpdateListener);

	// Call module's staticInit function to create default static config.
	if (info->functions->moduleStaticInit != nullptr) {
		try {
			info->functions->moduleStaticInit(static_cast<dvConfigNode>(moduleNode));
		}
		catch (const std::exception &ex) {
			boost::format exMsg = boost::format("%s: moduleStaticInit() failed, error '%s'.") % name % ex.what();
			dv::Log(dv::logLevel::ERROR, exMsg);
			throw std::invalid_argument(exMsg.str().c_str());
		}
	}

	// Each module can set priority attributes for UI display. By default let's show 'logLevel'.
	// Called last to allow for configInit() function to create a different default first.
	moduleNode.attributeModifierPriorityAttributes("logLevel");
}

void Module::handleModuleInitFailure(dvCfg::Node &moduleNode) {
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

void Module::runStateMachine() {
	dv::LoggerSet(&logger);

	bool localRunning = running.load(std::memory_order_relaxed);
	dvCfg::Node moduleNode(data.moduleNode);

	if (moduleStatus == ModuleStatus::RUNNING && localRunning) {
		if (configUpdate.load(std::memory_order_relaxed) != 0) {
			configUpdate.store(0);

			if (info->functions->moduleConfig != nullptr) {
				// Call config function. 'configUpdate' variable reset is done above.
				try {
					info->functions->moduleConfig(&data);
				}
				catch (const std::exception &ex) {
					dv::Log(dv::logLevel::ERROR, "moduleConfig(): '%s', disabling module.", ex.what());

					moduleNode.put<dvCfgType::BOOL>("running", false);
					return;
				}
			}
		}

		if (info->functions->moduleRun != nullptr) {
			try {
				info->functions->moduleRun(&data);
			}
			catch (const std::exception &ex) {
				dv::Log(dv::logLevel::ERROR, "moduleRun(): '%s', disabling module.", ex.what());

				moduleNode.put<dvCfgType::BOOL>("running", false);
				return;
			}
		}
	}
	else if (moduleStatus == ModuleStatus::STOPPED && localRunning) {
		// Allocate memory for module state.
		if (info->memSize != 0) {
			data.moduleState = calloc(1, info->memSize);
			if (data.moduleState == nullptr) {
				dv::Log(dv::logLevel::ERROR, "moduleInit(): '%s', disabling module.", "memory allocation failure");

				handleModuleInitFailure(moduleNode);
				return;
			}
		}
		else {
			// memSize is zero, so moduleState must be nullptr.
			data.moduleState = nullptr;
		}

		// Reset variables, as the following Init() is stronger than a reset
		// and implies a full configuration update. This avoids stale state
		// forcing an update and/or reset right away in the first run of
		// the module, which is unneeded and wasteful.
		configUpdate.store(0);

		if (info->functions->moduleInit != nullptr) {
			try {
				if (!info->functions->moduleInit(&data)) {
					throw std::runtime_error("Failed to initialize module.");
				}
			}
			catch (const std::exception &ex) {
				dv::Log(dv::logLevel::ERROR, "moduleInit(): '%s', disabling module.", ex.what());

				if (info->memSize != 0) {
					// Only deallocate if we were the original allocator.
					free(data.moduleState);
				}
				data.moduleState = nullptr;

				handleModuleInitFailure(moduleNode);
				return;
			}
		}

		moduleStatus = ModuleStatus::RUNNING;
		moduleNode.updateReadOnly<dvCfgType::BOOL>("isRunning", true);
	}
	else if (moduleStatus == ModuleStatus::RUNNING && !localRunning) {
		moduleStatus = ModuleStatus::STOPPED;

		if (info->functions->moduleExit != nullptr) {
			try {
				info->functions->moduleExit(&data);
			}
			catch (const std::exception &ex) {
				dv::Log(dv::logLevel::ERROR, "moduleExit(): '%s'.", ex.what());
			}
		}

		if (info->memSize != 0) {
			// Only deallocate if we were the original allocator.
			free(data.moduleState);
		}
		data.moduleState = nullptr;

		moduleNode.updateReadOnly<dvCfgType::BOOL>("isRunning", false);
	}
}

void Module::moduleShutdownListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	auto data = static_cast<std::atomic_bool *>(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "running")) {
		data->store(changeValue.boolean);
	}
}

void Module::moduleLogLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	auto data = static_cast<std::atomic_int32_t *>(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "logLevel")) {
		data->store(changeValue.iint);
	}
}

void Module::moduleConfigUpdateListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);
	UNUSED_ARGUMENT(changeKey);
	UNUSED_ARGUMENT(changeType);
	UNUSED_ARGUMENT(changeValue);

	auto data = static_cast<std::atomic_uint32_t *>(userData);

	// Simply set the config update flag to 1 on any attribute change.
	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		data->store(1);
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

		// Done, unload library.
		dvModuleUnloadLibrary(mLoad.first);

		iter++;
	}
}
