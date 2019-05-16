#include "devices_discovery.hpp"

#include <libcaercpp/devices/device_discover.hpp>

#include <boost/format.hpp>
#include <mutex>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

// Available devices list support.
static std::mutex glAvailableDevicesLock;

void dv::DevicesUpdateListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL
		&& caerStrEquals(changeKey, "updateAvailableDevices") && changeValue.boolean) {
		// Get information on available devices, put it into ConfigTree.
		DevicesUpdateList();

		dvConfigNodeAttributeButtonReset(node, changeKey);
	}
}

void dv::DevicesUpdateList() {
	std::scoped_lock lock(glAvailableDevicesLock);

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
					devNode.create<dvCfgType::INT>("FirmwareVersion", info->firmwareVersion, {0, INT16_MAX},
						dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Version of device firmware.");
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
