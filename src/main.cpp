#include "main.hpp"

#include "dv-sdk/cross/portable_io.h"

#include "config.hpp"
#include "config_server/config_server_main.hpp"
#include "devices_discovery.hpp"
#include "log.hpp"
#include "module.hpp"
#include "modules_discovery.hpp"
#include "service.hpp"

#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>

// If Boost version recent enough, enable better stack traces on segfault.
#include <boost/version.hpp>

#if defined(BOOST_VERSION) && (BOOST_VERSION / 100000) == 1 && (BOOST_VERSION / 100 % 1000) >= 66
#	define BOOST_HAS_STACKTRACE 1
#else
#	define BOOST_HAS_STACKTRACE 0
#endif

#if BOOST_HAS_STACKTRACE
#	define BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED 1
#	include <boost/stacktrace.hpp>
#elif defined(OS_LINUX)
#	include <execinfo.h>
#endif

#define INTERNAL_XSTR(a) INTERNAL_STR(a)
#define INTERNAL_STR(a) #a

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

static void mainRunner();
static void mainSegfaultHandler(int signum);
static void mainShutdownHandler(int signum);
static void systemRunningListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

void dv::addModule(const std::string &name, const std::string &library) {
	std::scoped_lock lock(dv::MainData::getGlobal().modulesLock);

	auto restoreLogger = dv::LoggerGet();
	dv::MainData::getGlobal().modules.try_emplace(name, std::make_shared<dv::Module>(name, library));
	dv::LoggerSet(restoreLogger);
}

void dv::removeModule(const std::string &name) {
	std::scoped_lock lock(dv::MainData::getGlobal().modulesLock);

	auto restoreLogger = dv::LoggerGet();
	dv::MainData::getGlobal().modules.erase(name);
	dv::LoggerSet(restoreLogger);
}

static void mainRunner() {
	// Setup internal function pointers for public support library.
	auto libFuncPtrs = &dv::MainData::getGlobal().libFunctionPointers;

	libFuncPtrs->getTypeInfoCharString = [typeSystem = &dv::MainData::getGlobal().typeSystem](const char *cs,
											 const dv::Module *m) { return (typeSystem->getTypeInfo(cs, m)); };
	libFuncPtrs->getTypeInfoIntegerID  = [typeSystem = &dv::MainData::getGlobal().typeSystem](uint32_t ii,
                                            const dv::Module *m) { return (typeSystem->getTypeInfo(ii, m)); };

	libFuncPtrs->registerType         = &dv::Module::registerType;
	libFuncPtrs->registerOutput       = &dv::Module::registerOutput;
	libFuncPtrs->registerInput        = &dv::Module::registerInput;
	libFuncPtrs->outputAllocate       = &dv::Module::outputAllocate;
	libFuncPtrs->outputCommit         = &dv::Module::outputCommit;
	libFuncPtrs->inputGet             = &dv::Module::inputGet;
	libFuncPtrs->inputDismiss         = &dv::Module::inputDismiss;
	libFuncPtrs->outputGetInfoNode    = &dv::Module::outputGetInfoNode;
	libFuncPtrs->inputGetUpstreamNode = &dv::Module::inputGetUpstreamNode;
	libFuncPtrs->inputGetInfoNode     = &dv::Module::inputGetInfoNode;
	libFuncPtrs->inputIsConnected     = &dv::Module::inputIsConnected;

	dv::SDKLibInit(libFuncPtrs);

// Install signal handler for global shutdown.
#if defined(OS_WINDOWS)
	if (signal(SIGTERM, &mainShutdownHandler) == SIG_ERR) {
		dv::Log(dv::logLevel::EMERGENCY, "Failed to set signal handler for SIGTERM. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	if (signal(SIGINT, &mainShutdownHandler) == SIG_ERR) {
		dv::Log(dv::logLevel::EMERGENCY, "Failed to set signal handler for SIGINT. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	if (signal(SIGBREAK, &mainShutdownHandler) == SIG_ERR) {
		dv::Log(dv::logLevel::EMERGENCY, "Failed to set signal handler for SIGBREAK. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	if (signal(SIGSEGV, &mainShutdownHandler) == SIG_ERR) {
		dv::Log(dv::logLevel::EMERGENCY, "Failed to set signal handler for SIGSEGV. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	if (signal(SIGABRT, &mainShutdownHandler) == SIG_ERR) {
		dv::Log(dv::logLevel::EMERGENCY, "Failed to set signal handler for SIGABRT. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	// Disable closing of the console window where DV is executing.
	// While we do catch the signal (SIGBREAK) that such an action generates, it seems
	// we can't reliably shut down within the hard time window that Windows enforces when
	// pressing the close button (X in top right corner usually). This seems to be just
	// 5 seconds, and we can't guarantee full shutdown (USB, file writing, etc.) in all
	// cases within that time period (multiple cameras, modules etc. make this worse).
	// So we just disable that and force the user to CTRL+C, which works fine.
	HWND consoleWindow = GetConsoleWindow();
	if (consoleWindow != nullptr) {
		HMENU systemMenu = GetSystemMenu(consoleWindow, false);
		EnableMenuItem(systemMenu, SC_CLOSE, MF_GRAYED);
	}
#else
	struct sigaction shutdown;

	shutdown.sa_handler = &mainShutdownHandler;
	shutdown.sa_flags   = 0;
	sigemptyset(&shutdown.sa_mask);
	sigaddset(&shutdown.sa_mask, SIGTERM);
	sigaddset(&shutdown.sa_mask, SIGINT);

	if (sigaction(SIGTERM, &shutdown, nullptr) == -1) {
		dv::Log(dv::logLevel::EMERGENCY, "Failed to set signal handler for SIGTERM. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	if (sigaction(SIGINT, &shutdown, nullptr) == -1) {
		dv::Log(dv::logLevel::EMERGENCY, "Failed to set signal handler for SIGINT. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	struct sigaction segfault;

	segfault.sa_handler = &mainSegfaultHandler;
	segfault.sa_flags   = 0;
	sigemptyset(&segfault.sa_mask);
	sigaddset(&segfault.sa_mask, SIGSEGV);
	sigaddset(&segfault.sa_mask, SIGABRT);

	if (sigaction(SIGSEGV, &segfault, nullptr) == -1) {
		dv::Log(dv::logLevel::EMERGENCY, "Failed to set signal handler for SIGSEGV. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	if (sigaction(SIGABRT, &segfault, nullptr) == -1) {
		dv::Log(dv::logLevel::EMERGENCY, "Failed to set signal handler for SIGABRT. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	// Ignore SIGPIPE.
	signal(SIGPIPE, SIG_IGN);
#endif

	// Ensure core nodes exist.
	auto systemNode   = dvCfg::GLOBAL.getNode("/system/");
	auto mainloopNode = dvCfg::GLOBAL.getNode("/mainloop/");

	// Support device discovery.
	auto devicesNode = systemNode.getRelativeNode("devices/");

	devicesNode.create<dvCfgType::BOOL>("updateAvailableDevices", false, {}, dvCfgFlags::NORMAL | dvCfgFlags::NO_EXPORT,
		"Update available devices list.");
	devicesNode.attributeModifierButton("updateAvailableDevices", "EXECUTE");
	devicesNode.addAttributeListener(nullptr, &dv::DevicesUpdateListener);

	dv::DevicesUpdateList(); // Run once at startup.

	// Initialize module related configuration.
	auto modulesNode = systemNode.getRelativeNode("modules/");

	// Default search directories.
	boost::filesystem::path modulesDefaultDir(INTERNAL_XSTR(DV_MODULES_DIR));

	modulesNode.create<dvCfgType::STRING>("modulesSearchPath", modulesDefaultDir.string(), {1, 16 * PATH_MAX},
		dvCfgFlags::NORMAL, "Directories to search loadable modules in, separated by '|'.");

	modulesNode.create<dvCfgType::BOOL>("updateModulesInformation", false, {},
		dvCfgFlags::NORMAL | dvCfgFlags::NO_EXPORT, "Update modules information.");
	modulesNode.attributeModifierButton("updateModulesInformation", "EXECUTE");
	modulesNode.addAttributeListener(nullptr, &dv::ModulesUpdateInformationListener);

	dv::ModulesUpdateInformation();

	// Allow user-driven configuration write-back.
	systemNode.create<dvCfgType::BOOL>("writeConfiguration", false, {}, dvCfgFlags::NORMAL | dvCfgFlags::NO_EXPORT,
		"Write current configuration to XML config file.");
	systemNode.attributeModifierButton("writeConfiguration", "EXECUTE");
	systemNode.addAttributeListener(nullptr, &dv::ConfigWriteBackListener);

	// Allow system running status control (shutdown).
	systemNode.create<dvCfgType::BOOL>(
		"running", true, {}, dvCfgFlags::NORMAL | dvCfgFlags::NO_EXPORT, "Global system start/stop.");
	systemNode.addAttributeListener(nullptr, &systemRunningListener);

	// Add each module defined in configuration to runnable modules.
	for (const auto &child : mainloopNode.getChildren()) {
		dv::addModule(child.getName(), child.get<dvCfgType::STRING>("moduleLibrary"));
	}

	// Start the configuration server thread for run-time config changes.
	dv::ConfigServerStart();

	// Main thread now works as updater (sleeps most of the time).
	while (dv::MainData::getGlobal().systemRunning.load(std::memory_order_relaxed)) {
		dvCfg::GLOBAL.attributeUpdaterRun();

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	// After shutting down the updater, also shutdown the config server thread.
	dv::ConfigServerStop();

	// We don't remove modules here, as that would delete their configuration.
	// But we do make sure they're all properly shut down.
	for (auto &child : mainloopNode.getChildren()) {
		child.put<dvCfgType::BOOL>("running", false);
	}

	// Wait for termination ...
	for (const auto &child : mainloopNode.getChildren()) {
		while (child.get<dvCfgType::BOOL>("isRunning")) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	// Remove attribute listeners for clean shutdown.
	systemNode.removeAttributeListener(nullptr, &systemRunningListener);
	systemNode.removeAttributeListener(nullptr, &dv::ConfigWriteBackListener);
	modulesNode.removeAttributeListener(nullptr, &dv::ModulesUpdateInformationListener);
	devicesNode.removeAttributeListener(nullptr, &dv::DevicesUpdateListener);

	// Write config back on shutdown.
	dv::ConfigWriteBack();
}

static void mainSegfaultHandler(int signum) {
	signal(signum, SIG_DFL);

// Segfault or abnormal termination, try to print a stack trace if possible.
#if BOOST_HAS_STACKTRACE
	std::cerr << boost::stacktrace::stacktrace();
#elif defined(OS_LINUX)
	void *traces[128];
	int tracesActualNum = backtrace(traces, 128);
	backtrace_symbols_fd(traces, tracesActualNum, STDERR_FILENO);
#endif

	raise(signum);
}

static void mainShutdownHandler(int signum) {
	UNUSED_ARGUMENT(signum);

	// Simply set all the running flags to false on SIGTERM and SIGINT (CTRL+C) for global shutdown.
	dv::MainData::getGlobal().systemRunning.store(false);
}

static void systemRunningListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);
	UNUSED_ARGUMENT(userData);
	UNUSED_ARGUMENT(changeValue);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "running")) {
		dv::MainData::getGlobal().systemRunning.store(false);
	}
}

int main(int argc, char **argv) {
	// Initialize config storage from file, support command-line overrides.
	dv::ConfigInit(argc, argv);

	// Initialize logging sub-system.
	dv::LoggerInit();

	// Start the DV runtime. Can be as a background service or console application.
	dv::ServiceInit(&mainRunner);

	return (EXIT_SUCCESS);
}
