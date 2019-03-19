#ifndef MODULE_H_
#define MODULE_H_

#include "dv-sdk/module.h"

#include "log.hpp"

#include <string>
#include <utility>

// If Boost version recent enough, use their portable DLL loading support.
// Else use dlopen() on POSIX systems.
#include <boost/version.hpp>
#if defined(BOOST_VERSION) && (BOOST_VERSION / 100000) == 1 && (BOOST_VERSION / 100 % 1000) >= 61
#	define BOOST_HAS_DLL_LOAD 1
#else
#	define BOOST_HAS_DLL_LOAD 0
#endif

#if BOOST_HAS_DLL_LOAD
#	include <boost/dll.hpp>
using ModuleLibrary = boost::dll::shared_library;
#else
#	include <dlfcn.h>
using ModuleLibrary = void *;
#endif

std::pair<ModuleLibrary, dvModuleInfo> dvModuleLoadLibrary(const std::string &moduleName);
void dvModuleUnloadLibrary(ModuleLibrary &moduleLibrary);
void dvUpdateModulesInformation();

// Module-related definitions.
enum class ModuleStatus {
	STOPPED = 0,
	RUNNING = 1,
};

class ModuleOutput {
	dv::Types::Type type;
};

class ModuleInput {
	dv::Types::Type type;
	bool optional;
};

class Module {
private:
	std::string name;
	dvModuleInfo info;
	ModuleLibrary library;
	ModuleStatus moduleStatus;
	std::atomic_bool running;
	std::atomic_uint32_t configUpdate;
	dv::LogBlock logger;
	std::unordered_map<std::string, ModuleOutput> outputs;
	std::unordered_map<std::string, ModuleInput> inputs;
	dvModuleDataS data;

public:
	Module(const std::string &_name, const std::string &_library);
	~Module();

	void runStateMachine();

private:
	void LoggingInit(dv::Config::Node &moduleNode);
	void RunningInit(dv::Config::Node &moduleNode);
	void StaticInit(dv::Config::Node &moduleNode);

	static void handleModuleInitFailure(dv::Config::Node &moduleNode);

	static void moduleShutdownListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
	static void moduleLogLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
	static void moduleConfigUpdateListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) :
};

#endif /* MODULE_H_ */
