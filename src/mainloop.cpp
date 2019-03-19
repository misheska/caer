#include "mainloop.h"

#include <libcaercpp/devices/device_discover.hpp>

#include "dv-sdk/cross/portable_io.h"

#include "config.h"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/range/join.hpp>
#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <queue>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_set>

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

// MAINLOOP DATA GLOBAL VARIABLE.
static dv::MainData glMainData;

static void mainRunner();

static void mainSegfaultHandler(int signum);
static void mainShutdownHandler(int signum);
static void systemRunningListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void writeConfigurationListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

static void updateAvailableDevicesListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
static void dvUpdateAvailableDevices();

void dv::MainRun(void) {
	// Setup internal mainloop pointer for public support library.
	dv::MainSDKLibInit(&glMainData);

// Install signal handler for global shutdown.
#if defined(OS_WINDOWS)
	if (signal(SIGTERM, &mainloopShutdownHandler) == SIG_ERR) {
		log(logLevel::EMERGENCY, "Mainloop", "Failed to set signal handler for SIGTERM. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	if (signal(SIGINT, &mainloopShutdownHandler) == SIG_ERR) {
		log(logLevel::EMERGENCY, "Mainloop", "Failed to set signal handler for SIGINT. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	if (signal(SIGBREAK, &mainloopShutdownHandler) == SIG_ERR) {
		log(logLevel::EMERGENCY, "Mainloop", "Failed to set signal handler for SIGBREAK. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	if (signal(SIGSEGV, &mainloopSegfaultHandler) == SIG_ERR) {
		log(logLevel::EMERGENCY, "Mainloop", "Failed to set signal handler for SIGSEGV. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	if (signal(SIGABRT, &mainloopSegfaultHandler) == SIG_ERR) {
		log(logLevel::EMERGENCY, "Mainloop", "Failed to set signal handler for SIGABRT. Error: %d.", errno);
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
		log(logLevel::EMERGENCY, "Mainloop", "Failed to set signal handler for SIGTERM. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	if (sigaction(SIGINT, &shutdown, nullptr) == -1) {
		log(logLevel::EMERGENCY, "Mainloop", "Failed to set signal handler for SIGINT. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	struct sigaction segfault;

	segfault.sa_handler = &mainSegfaultHandler;
	segfault.sa_flags   = 0;
	sigemptyset(&segfault.sa_mask);
	sigaddset(&segfault.sa_mask, SIGSEGV);
	sigaddset(&segfault.sa_mask, SIGABRT);

	if (sigaction(SIGSEGV, &segfault, nullptr) == -1) {
		log(logLevel::EMERGENCY, "Mainloop", "Failed to set signal handler for SIGSEGV. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	if (sigaction(SIGABRT, &segfault, nullptr) == -1) {
		log(logLevel::EMERGENCY, "Mainloop", "Failed to set signal handler for SIGABRT. Error: %d.", errno);
		exit(EXIT_FAILURE);
	}

	// Ignore SIGPIPE.
	signal(SIGPIPE, SIG_IGN);
#endif

	auto systemNode = dvCfg::GLOBAL.getNode("/system/");

	// Support device discovery.
	auto devicesNode = systemNode.getRelativeNode("devices/");

	devicesNode.create<dvCfgType::BOOL>("updateAvailableDevices", false, {}, dvCfgFlags::NORMAL | dvCfgFlags::NO_EXPORT,
		"Update available devices list.");
	devicesNode.attributeModifierButton("updateAvailableDevices", "EXECUTE");
	devicesNode.addAttributeListener(nullptr, &updateAvailableDevicesListener);

	dvUpdateAvailableDevices(); // Run once at startup.

	// Initialize module related configuration.
	auto modulesNode = systemNode.getRelativeNode("modules/");

	// Default search directories.
	boost::filesystem::path modulesDefaultDir(INTERNAL_XSTR(DV_MODULES_DIR));

	modulesNode.create<dvCfgType::STRING>("modulesSearchPath", modulesDefaultDir.string(), {1, 8 * PATH_MAX},
		dvCfgFlags::NORMAL, "Directories to search loadable modules in, separated by '|'.");

	modulesNode.create<dvCfgType::BOOL>("updateModulesInformation", false, {},
		dvCfgFlags::NORMAL | dvCfgFlags::NO_EXPORT, "Update modules information.");
	modulesNode.attributeModifierButton("updateModulesInformation", "EXECUTE");
	modulesNode.addAttributeListener(nullptr, &dv::ModulesUpdateInformationListener);

	// Allow user-driven configuration write-back.
	systemNode.create<dvCfgType::BOOL>("writeConfiguration", false, {}, dvCfgFlags::NORMAL | dvCfgFlags::NO_EXPORT,
		"Write current configuration to XML config file.");
	systemNode.attributeModifierButton("writeConfiguration", "EXECUTE");
	systemNode.addAttributeListener(nullptr, &writeConfigurationListener);

	// Allow system running status control (shutdown).
	systemNode.create<dvCfgType::BOOL>(
		"running", true, {}, dvCfgFlags::NORMAL | dvCfgFlags::NO_EXPORT, "Global system start/stop.");
	systemNode.addAttributeListener(nullptr, &systemRunningListener);
	glMainData.systemRunning.store(true);

	glMainData.configNode = dvCfg::GLOBAL.getNode("/mainloop/");

	while (glMainData.systemRunning.load()) {
		mainRunner();
	}

	// Remove attribute listeners for clean shutdown.
	systemNode.removeAttributeListener(nullptr, &systemRunningListener);
	modulesNode.removeAttributeListener(nullptr, &writeConfigurationListener);
	modulesNode.removeAttributeListener(nullptr, &dv::ModulesUpdateInformationListener);
	devicesNode.removeAttributeListener(nullptr, &updateAvailableDevicesListener);
}

static void mainRunner() {
}

static void mainSegfaultHandler(int signum) {
	signal(signum, SIG_DFL);

// Segfault or abnormal termination, try to print a stack trace if possible.
#if BOOST_HAS_STACKTRACE
	std::cout << boost::stacktrace::stacktrace();
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
	glMainData.systemRunning.store(false);
}

static void systemRunningListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);
	UNUSED_ARGUMENT(userData);
	UNUSED_ARGUMENT(changeValue);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "running")) {
		glMainData.systemRunning.store(false);
	}
}

static void writeConfigurationListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL
		&& caerStrEquals(changeKey, "writeConfiguration") && changeValue.boolean) {
		dvConfigWriteBack();

		dvConfigNodeAttributeButtonReset(node, changeKey);
	}
}

// Available devices list support.
static std::recursive_mutex glAvailableDevicesLock;

static void updateAvailableDevicesListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL
		&& caerStrEquals(changeKey, "updateAvailableDevices") && changeValue.boolean) {
		// Get information on available devices, put it into ConfigTree.
		dvUpdateAvailableDevices();

		dvConfigNodeAttributeButtonReset(node, changeKey);
	}
}

static void dvUpdateAvailableDevices() {
	std::lock_guard<std::recursive_mutex> lock(glAvailableDevicesLock);

	auto devicesNode = dvCfg::GLOBAL.getNode("/system/devices/");

	// Clear out current available devices information.
	devicesNode.clearSubTree(false);
	devicesNode.removeSubTree();

	const auto devices = libcaer::devices::discover::all();

	for (const auto &dev : devices) {
		switch (dev.deviceType) {
			case CAER_DEVICE_DVS128: {
				const struct caer_dvs128_info *info = &dev.deviceInfo.dvs128Info;

				auto nodeName = boost::format("dvs128_%d-%d/") % static_cast<int>(info->deviceUSBBusNumber)
								% static_cast<int>(info->deviceUSBDeviceAddress);

				auto devNode = devicesNode.getRelativeNode(nodeName.str());

				devNode.create<dvCfgType::STRING>("OpenWithModule", "dv_dvs128", {1, 32},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Open device with specified module.");

				devNode.create<dvCfgType::BOOL>("OpenError", dev.deviceErrorOpen, {},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Device cannot be opened (already in use).");
				devNode.create<dvCfgType::BOOL>("VersionError", dev.deviceErrorVersion, {},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Device has old firmware/logic versions.");

				devNode.create<dvCfgType::INT>("USBBusNumber", info->deviceUSBBusNumber, {0, 255},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "USB bus number.");
				devNode.create<dvCfgType::INT>("USBDeviceAddress", info->deviceUSBDeviceAddress, {0, 255},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "USB device address.");
				devNode.create<dvCfgType::STRING>("SerialNumber", info->deviceSerialNumber, {0, 8},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "USB device serial number.");

				if (!dev.deviceErrorOpen && !dev.deviceErrorVersion) {
					devNode.create<dvCfgType::INT>("LogicVersion", info->logicVersion, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Version of FPGA logic.");
					devNode.create<dvCfgType::BOOL>("DeviceIsMaster", info->deviceIsMaster, {},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Device is timestamp master.");

					devNode.create<dvCfgType::INT>("DVSSizeX", info->dvsSizeX, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "DVS X axis resolution.");
					devNode.create<dvCfgType::INT>("DVSSizeY", info->dvsSizeY, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "DVS Y axis resolution.");
				}

				break;
			}

			case CAER_DEVICE_DAVIS_FX2:
			case CAER_DEVICE_DAVIS_FX3:
			case CAER_DEVICE_DAVIS: {
				const struct caer_davis_info *info = &dev.deviceInfo.davisInfo;

				auto nodeName = boost::format("davis_%d-%d/") % static_cast<int>(info->deviceUSBBusNumber)
								% static_cast<int>(info->deviceUSBDeviceAddress);

				auto devNode = devicesNode.getRelativeNode(nodeName.str());

				devNode.create<dvCfgType::STRING>("OpenWithModule", "dv_davis", {1, 32},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Open device with specified module.");

				devNode.create<dvCfgType::BOOL>("OpenError", dev.deviceErrorOpen, {},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Device cannot be opened (already in use).");
				devNode.create<dvCfgType::BOOL>("VersionError", dev.deviceErrorVersion, {},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Device has old firmware/logic versions.");

				devNode.create<dvCfgType::INT>("USBBusNumber", info->deviceUSBBusNumber, {0, 255},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "USB bus number.");
				devNode.create<dvCfgType::INT>("USBDeviceAddress", info->deviceUSBDeviceAddress, {0, 255},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "USB device address.");
				devNode.create<dvCfgType::STRING>("SerialNumber", info->deviceSerialNumber, {0, 8},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "USB device serial number.");

				if (!dev.deviceErrorOpen && !dev.deviceErrorVersion) {
					devNode.create<dvCfgType::INT>("FirmwareVersion", info->firmwareVersion, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Version of device firmware.");
					devNode.create<dvCfgType::INT>("LogicVersion", info->logicVersion, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Version of FPGA logic.");
					devNode.create<dvCfgType::BOOL>("DeviceIsMaster", info->deviceIsMaster, {},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Device is timestamp master.");

					devNode.create<dvCfgType::INT>("DVSSizeX", info->dvsSizeX, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "DVS X axis resolution.");
					devNode.create<dvCfgType::INT>("DVSSizeY", info->dvsSizeY, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "DVS Y axis resolution.");

					devNode.create<dvCfgType::INT>("APSSizeX", info->apsSizeX, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Frames X axis resolution.");
					devNode.create<dvCfgType::INT>("APSSizeY", info->apsSizeY, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Frames Y axis resolution.");
					devNode.create<dvCfgType::STRING>("ColorMode",
						(info->apsColorFilter == MONO) ? ("Mono") : ("Color"), {4, 5},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Frames color mode.");
				}

				break;
			}

			case CAER_DEVICE_EDVS: {
				const struct caer_edvs_info *info = &dev.deviceInfo.edvsInfo;

				auto nodeName = boost::format("edvs_%s/") % info->serialPortName;

				auto devNode = devicesNode.getRelativeNode(nodeName.str());

				devNode.create<dvCfgType::STRING>("OpenWithModule", "dv_edvs", {1, 32},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Open device with specified module.");

				devNode.create<dvCfgType::BOOL>("OpenError", dev.deviceErrorOpen, {},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Device cannot be opened (already in use).");
				devNode.create<dvCfgType::BOOL>("VersionError", dev.deviceErrorVersion, {},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Device has old firmware/logic versions.");

				if (!dev.deviceErrorOpen && !dev.deviceErrorVersion) {
					devNode.create<dvCfgType::INT>("SerialBaudRate", I32T(info->serialBaudRate), {1, INT32_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Serial device baud rate (in baud).");
					devNode.create<dvCfgType::STRING>("SerialPortName", info->serialPortName, {1, 64},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT,
						"Serial device port name (COM1, /dev/ttyUSB1, ...).");

					devNode.create<dvCfgType::BOOL>("DeviceIsMaster", info->deviceIsMaster, {},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Device is timestamp master.");

					devNode.create<dvCfgType::INT>("DVSSizeX", info->dvsSizeX, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "DVS X axis resolution.");
					devNode.create<dvCfgType::INT>("DVSSizeY", info->dvsSizeY, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "DVS Y axis resolution.");
				}

				break;
			}

			case CAER_DEVICE_DVS132S: {
				const struct caer_dvs132s_info *info = &dev.deviceInfo.dvs132sInfo;

				auto nodeName = boost::format("dvs132s_%d-%d/") % static_cast<int>(info->deviceUSBBusNumber)
								% static_cast<int>(info->deviceUSBDeviceAddress);

				auto devNode = devicesNode.getRelativeNode(nodeName.str());

				devNode.create<dvCfgType::STRING>("OpenWithModule", "dv_dvs132s", {1, 32},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Open device with specified module.");

				devNode.create<dvCfgType::BOOL>("OpenError", dev.deviceErrorOpen, {},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Device cannot be opened (already in use).");
				devNode.create<dvCfgType::BOOL>("VersionError", dev.deviceErrorVersion, {},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Device has old firmware/logic versions.");

				devNode.create<dvCfgType::INT>("USBBusNumber", info->deviceUSBBusNumber, {0, 255},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "USB bus number.");
				devNode.create<dvCfgType::INT>("USBDeviceAddress", info->deviceUSBDeviceAddress, {0, 255},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "USB device address.");
				devNode.create<dvCfgType::STRING>("SerialNumber", info->deviceSerialNumber, {0, 8},
					dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "USB device serial number.");

				if (!dev.deviceErrorOpen && !dev.deviceErrorVersion) {
					devNode.create<dvCfgType::INT>("FirmwareVersion", info->firmwareVersion, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Version of device firmware.");
					devNode.create<dvCfgType::INT>("LogicVersion", info->logicVersion, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Version of FPGA logic.");
					devNode.create<dvCfgType::BOOL>("DeviceIsMaster", info->deviceIsMaster, {},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Device is timestamp master.");

					devNode.create<dvCfgType::INT>("DVSSizeX", info->dvsSizeX, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "DVS X axis resolution.");
					devNode.create<dvCfgType::INT>("DVSSizeY", info->dvsSizeY, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "DVS Y axis resolution.");
				}

				break;
			}

			case CAER_DEVICE_DYNAPSE:
			case CAER_DEVICE_DAVIS_RPI:
			default:
				// TODO: ignore for now.
				break;
		}
	}
}
