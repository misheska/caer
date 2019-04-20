#include "modules_discovery.hpp"

#include "dv-sdk/cross/portable_io.h"

#include "log.hpp"

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <mutex>
#include <regex>
#include <vector>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

static std::pair<dv::ModuleLibrary, dvModuleInfo> InternalLoadLibrary(boost::filesystem::path modulePath);

static struct {
	std::vector<boost::filesystem::path> modulePaths;
	std::mutex modulePathsMutex;
} glModuleData;

std::pair<dv::ModuleLibrary, dvModuleInfo> dv::ModulesLoadLibrary(std::string_view moduleName) {
	// For each module, we search if a path exists to load it from.
	// If yes, we do so. The various OS's shared library load mechanisms
	// will keep track of reference count if same module is loaded
	// multiple times.
	boost::filesystem::path modulePath;

	{
		std::scoped_lock lock(glModuleData.modulePathsMutex);

		for (const auto &p : glModuleData.modulePaths) {
			if (moduleName == p.stem().string()) {
				// Found a module with same name!
				modulePath = p;
				break;
			}
		}
	}

	if (modulePath.empty()) {
		boost::format exMsg = boost::format("No module library for '%s' found.") % moduleName;
		throw std::runtime_error(exMsg.str());
	}

	return (InternalLoadLibrary(modulePath));
}

static std::pair<dv::ModuleLibrary, dvModuleInfo> InternalLoadLibrary(boost::filesystem::path modulePath) {
#if BOOST_HAS_DLL_LOAD
	dv::ModuleLibrary moduleLibrary;
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
		dv::ModulesUnloadLibrary(moduleLibrary);
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
		dv::ModulesUnloadLibrary(moduleLibrary);
		boost::format exMsg
			= boost::format("Failed to find symbol in library '%s', error: '%s'.") % modulePath.string() % dlerror();
		throw std::runtime_error(exMsg.str());
	}
#endif

	dvModuleInfo info = (*getInfo)();
	if (info == nullptr) {
		dv::ModulesUnloadLibrary(moduleLibrary);
		boost::format exMsg = boost::format("Failed to get info from library '%s'.") % modulePath.string();
		throw std::runtime_error(exMsg.str());
	}

	return (std::pair<dv::ModuleLibrary, dvModuleInfo>(moduleLibrary, info));
}

// Small helper to unload libraries on error.
void dv::ModulesUnloadLibrary(dv::ModuleLibrary &moduleLibrary) {
#if BOOST_HAS_DLL_LOAD
	moduleLibrary.unload();
#else
	dlclose(moduleLibrary);
#endif
}

void dv::ModulesUpdateInformationListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL
		&& caerStrEquals(changeKey, "updateModulesInformation") && changeValue.boolean) {
		// Get information on available modules, put it into ConfigTree.
		try {
			ModulesUpdateInformation();
		}
		catch (const std::exception &ex) {
			dv::Log(dv::logLevel::CRITICAL, "Failed to find any modules (error: '%s').", ex.what());
		}

		dvConfigNodeAttributeButtonReset(node, changeKey);
	}
}

void dv::ModulesUpdateInformation() {
	std::scoped_lock lock(glModuleData.modulePathsMutex);

	auto modulesNode = dvCfg::GLOBAL.getNode("/system/modules/");

	// Clear out modules information.
	modulesNode.clearSubTree(false);
	modulesNode.removeSubTree();
	glModuleData.modulePaths.clear();

	// Search for available modules. Will be loaded as needed later.
	std::string modulesSearchPath = modulesNode.get<dvCfgType::STRING>("modulesSearchPath");

	char *execLocation = portable_get_executable_location();
	if (execLocation != nullptr) {
		boost::filesystem::path execPath(execLocation);
		free(execLocation);

		if (!boost::filesystem::is_directory(execPath)) {
			execPath.remove_filename();
		}

		execPath.append("dv_modules");

		modulesSearchPath = execPath.string() + "|" + modulesSearchPath;
	}

	// Split on '|'.
	boost::tokenizer<boost::char_separator<char>> searchPaths(
		modulesSearchPath, boost::char_separator<char>("|", nullptr));

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
	else {
		dv::Log(dv::logLevel::DEBUG, "Found %d modules on path(s) '%s'.", glModuleData.modulePaths.size(),
			modulesSearchPath.c_str());
	}

	// Generate nodes for each module, with their in/out information as attributes.
	// This also checks basic validity of the module's information.
	auto iter = glModuleData.modulePaths.cbegin();

	while (iter != glModuleData.modulePaths.cend()) {
		std::string moduleName = iter->stem().string();

		// Load library.
		std::pair<dv::ModuleLibrary, dvModuleInfo> mLoad;

		try {
			mLoad = InternalLoadLibrary(*iter);
		}
		catch (const std::exception &ex) {
			auto exMsg = boost::format("Module '%s': %s") % moduleName % ex.what();
			dv::Log(dv::logLevel::ERROR, exMsg);

			iter = glModuleData.modulePaths.erase(iter);
			continue;
		}

		// Get ConfigTree node under /system/modules/.
		if (modulesNode.existsRelativeNode(moduleName + "/")) {
			// Remove duplicates.
			auto exMsg = boost::format("Module '%s': removing duplicate '%s'.") % moduleName % iter->string();
			dv::Log(dv::logLevel::INFO, exMsg);

			dv::ModulesUnloadLibrary(mLoad.first);
			iter = glModuleData.modulePaths.erase(iter);
			continue;
		}

		auto moduleNode = modulesNode.getRelativeNode(moduleName + "/");

		// Parse dvModuleInfo into ConfigTree.
		moduleNode.create<dvCfgType::INT>("version", I32T(mLoad.second->version), {0, INT32_MAX},
			dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Module version.");
		moduleNode.create<dvCfgType::STRING>("description", mLoad.second->description, {1, 8192},
			dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Module description.");
		moduleNode.create<dvCfgType::STRING>("path", iter->string(), {1, PATH_MAX},
			dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Module file full path.");

		// Done, unload library.
		dv::ModulesUnloadLibrary(mLoad.first);

		iter++;
	}
}
