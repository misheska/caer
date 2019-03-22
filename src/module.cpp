#include "module.hpp"

#include "dv-sdk/cross/portable_io.h"

#include <algorithm>
#include <boost/format.hpp>
#include <regex>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

dv::Module::Module(std::string_view _name, std::string_view _library, dv::MainData *_mainData) :
	name(_name),
	moduleStatus(ModuleStatus::STOPPED),
	running(false),
	configUpdate(0),
	mainData(_mainData),
	moduleNode(nullptr) {
	// Load library to get module functions.
	try {
		std::tie(library, info) = dv::ModulesLoadLibrary(_library);
	}
	catch (const std::exception &ex) {
		boost::format exMsg = boost::format("%s: module library load failed, error '%s'.") % name % ex.what();
		dv::Log(dv::logLevel::ERROR, exMsg);
		throw std::invalid_argument(exMsg.str().c_str());
	}

	// Set configuration node (so it's user accessible).
	moduleNode = dvCfg::GLOBAL.getNode("/mainloop/" + name + "/");

	data.moduleNode = static_cast<dvConfigNode>(moduleNode);

	// State allocated later by init().
	data.moduleState = nullptr;

	// Ensure the library is stored for successive startups.
	moduleNode.create<dvCfgType::STRING>(
		"moduleLibrary", std::string(_library), {1, PATH_MAX}, dvCfgFlags::READ_ONLY, "Module library.");

	// Initialize logging related functionality.
	LoggingInit();

	// Initialize running related functionality.
	RunningInit();

	// Ensure static configuration is created on each module initialization.
	StaticInit();
}

dv::Module::~Module() {
	moduleNode.removeNode();

	mainData->typeSystem.unregisterModuleTypes(this);

	dv::ModulesUnloadLibrary(library);
}

void dv::Module::LoggingInit() {
	// Per-module custom log string prefix.
	logger.logPrefix = name;

	// Per-module log level support. Initialize with global log level value.
	moduleNode.create<dvCfgType::INT>("logLevel", caerLogLevelGet(), {CAER_LOG_EMERGENCY, CAER_LOG_DEBUG},
		dvCfgFlags::NORMAL, "Module-specific log-level.");

	logger.logLevel.store(moduleNode.get<dvCfgType::INT>("logLevel"));
	moduleNode.addAttributeListener(&logger.logLevel, &moduleLogLevelListener);

	// Switch to current module logger.
	dv::LoggerSet(&logger);
}

void dv::Module::RunningInit() {
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

void dv::Module::StaticInit() {
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

	// Each module can set priority attributes for UI display. By default let's show 'running'.
	// Called last to allow for configInit() function to create a different default first.
	moduleNode.attributeModifierPriorityAttributes("running");
}

dv::Config::Node dv::Module::getConfigNode() {
	return (moduleNode);
}

void dv::Module::registerType(const dv::Types::Type type) {
	mainData->typeSystem.registerModuleType(this, type);
}

void dv::Module::registerInput(std::string_view inputName, std::string_view typeName, bool optional) {
	auto typeInfo = mainData->typeSystem.getTypeInfo(typeName, this);

	std::string inputNameString(inputName);

	if (inputs.count(inputNameString)) {
		throw std::invalid_argument("Input with name '" + inputNameString + "' already exists.");
	}

	// Add info to internal data structure.
	inputs.try_emplace(inputNameString, typeInfo, optional);

	// Add info to ConfigTree.
	auto inputNode = moduleNode.getRelativeNode("inputs/" + inputNameString + "/");

	inputNode.create<dvCfgType::BOOL>("optional", optional, {}, dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT,
		"Module can run without this input being connected.");
	inputNode.create<dvCfgType::STRING>("typeIdentifier", typeInfo.identifier, {4, 4},
		dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Type identifier.");
	inputNode.create<dvCfgType::STRING>("typeDescription", typeInfo.description, {1, 200},
		dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Type description.");

	// Add connectivity configuration attribute.
	inputNode.create<dvCfgType::STRING>(
		"from", "", {0, 256}, dvCfgFlags::NORMAL, "From which 'moduleName[outputName]' to get data.");
}

void dv::Module::registerOutput(std::string_view outputName, std::string_view typeName) {
	auto typeInfo = mainData->typeSystem.getTypeInfo(typeName, this);

	std::string outputNameString(outputName);

	if (outputs.count(outputNameString)) {
		throw std::invalid_argument("Output with name '" + outputNameString + "' already exists.");
	}

	// Add info to internal data structure.
	outputs.try_emplace(outputNameString, typeInfo);

	// Add info to ConfigTree.
	auto outputNode = moduleNode.getRelativeNode("outputs/" + outputNameString + "/");

	outputNode.create<dvCfgType::STRING>("typeIdentifier", typeInfo.identifier, {4, 4},
		dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Type identifier.");
	outputNode.create<dvCfgType::STRING>("typeDescription", typeInfo.description, {1, 200},
		dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Type description.");
}

static const std::regex inputConnRegex("^([a-zA-Z-_\\d\\.]+)\\[([a-zA-Z-_\\d\\.]+)\\]$");

bool dv::Module::handleInputConnectivity() {
	for (auto &input : inputs) {
		// Get current module connectivity configuration.
		auto inputNode = moduleNode.getRelativeNode("inputs/" + input.first + "/");
		auto inputConn = inputNode.get<dvCfgType::STRING>("from");

		// Check basic syntax: either empty or 'x[y]'.
		if (inputConn.empty()) {
			if (input.second.optional) {
				// Fine if optional, just skip this input then.
				continue;
			}
			else {
				// Not optional, must be defined!
				auto msg = boost::format(
							   "Input '%s': input is not optional, its connectivity attribute can not be left empty.")
						   % input.first;
				dv::Log(dv::logLevel::ERROR, msg);
				return (false);
			}
		}

		// Not empty, so check syntax and then components.
		std::smatch inputConnComponents;
		if (!std::regex_match(inputConn, inputConnComponents, inputConnRegex)) {
			auto msg
				= boost::format("Input '%s': invalid format of connectivity attribute '%s'.") % input.first % inputConn;
			dv::Log(dv::logLevel::ERROR, msg);
			return (false);
		}

		auto moduleName = inputConnComponents.str(1);
		auto outputName = inputConnComponents.str(2);

		// Does the referenced module exist?
		if (!mainData->modules.count(moduleName)) {
			auto msg = boost::format("Input '%s': invalid connectivity attribute, module '%s' doesn't exist.")
					   % input.first % moduleName;
			dv::Log(dv::logLevel::ERROR, msg);
			return (false);
		}

		auto otherModule = mainData->modules[moduleName];

		// Does it have the specified output?
		auto moduleOutput = otherModule->getModuleOutput(outputName);
		if (moduleOutput == nullptr) {
			auto msg
				= boost::format("Input '%s': invalid connectivity attribute, output '%s' doesn't exist in module '%s'.")
				  % input.first % outputName % moduleName;
			dv::Log(dv::logLevel::ERROR, msg);
			return (false);
		}

		// Lastly, check the type.
		if (input.second.type.id != moduleOutput->type.id) {
			auto msg = boost::format("Input '%s': invalid connectivity attribute, output '%s' in module '%s' has type "
									 "'%s', but this input requires type '%s'.")
					   % input.first % outputName % moduleName % moduleOutput->type.identifier % input.second.type.id;
			dv::Log(dv::logLevel::ERROR, msg);
			return (false);
		}

		// All is well, let's connect to that output.
		otherModule->connectToModuleOutput(moduleOutput, input.second.queue);
	}

	return (true);
}

dv::ModuleOutput *dv::Module::getModuleOutput(const std::string &outputName) {
	try {
		return (&outputs.at(outputName));
	}
	catch (const std::out_of_range &) {
		// Fine, not fund.
		return (nullptr);
	}
}

void dv::Module::connectToModuleOutput(ModuleOutput *output, libcaer::ringbuffer::RingBuffer &destinationQueue) {
	std::scoped_lock lock(output->destinationsLock);

	output->destinations.push_back(destinationQueue);
}

void dv::Module::handleModuleInitFailure() {
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

void dv::Module::runStateMachine() {
	dv::LoggerSet(&logger);

	bool localRunning = running.load(std::memory_order_relaxed);

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
		// Serialize module start/stop globally.
		std::scoped_lock lock(mainData->modulesLock);

		// At module startup, first check that input connectivity is
		// satisfied and hook up the input queues.
		if (!handleInputConnectivity()) {
			dv::Log(dv::logLevel::ERROR, "moduleInit(): '%s', disabling module.", "input connectivity failure");

			handleModuleInitFailure();
			return;
		}

		// Allocate memory for module state.
		if (info->memSize != 0) {
			data.moduleState = calloc(1, info->memSize);
			if (data.moduleState == nullptr) {
				dv::Log(dv::logLevel::ERROR, "moduleInit(): '%s', disabling module.", "memory allocation failure");

				handleModuleInitFailure();
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

				handleModuleInitFailure();
				return;
			}
		}

		moduleStatus = ModuleStatus::RUNNING;
		moduleNode.updateReadOnly<dvCfgType::BOOL>("isRunning", true);
	}
	else if (moduleStatus == ModuleStatus::RUNNING && !localRunning) {
		// Serialize module start/stop globally.
		std::scoped_lock lock(mainData->modulesLock);

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

void dv::Module::moduleShutdownListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	auto data = static_cast<std::atomic_bool *>(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "running")) {
		data->store(changeValue.boolean);
	}
}

void dv::Module::moduleLogLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	auto data = static_cast<std::atomic_int32_t *>(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "logLevel")) {
		data->store(changeValue.iint);
	}
}

void dv::Module::moduleConfigUpdateListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
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
