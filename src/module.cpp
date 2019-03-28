#include "module.hpp"

#include "dv-sdk/cross/portable_io.h"

#include <algorithm>
#include <boost/format.hpp>
#include <chrono>
#include <regex>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

dv::Module::Module(std::string_view _name, std::string_view _library) :
	name(_name),
	running(false),
	isRunning(false),
	configUpdate(false),
	dataAvailable(0),
	threadAlive(false) {
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
	auto moduleConfigNode = dvCfg::GLOBAL.getNode("/mainloop/" + name + "/");

	moduleNode = static_cast<dvConfigNode>(moduleConfigNode);

	// State allocated later by init().
	moduleState = nullptr;

	// Ensure the library is stored for successive startups.
	moduleConfigNode.create<dvCfgType::STRING>(
		"moduleLibrary", std::string(_library), {1, PATH_MAX}, dvCfgFlags::READ_ONLY, "Module library.");

	// Initialize logging related functionality.
	LoggingInit();

	// Initialize running related functionality.
	RunningInit();

	// Ensure static configuration is created on each module initialization.
	StaticInit();

	// Start module thread.
	threadAlive.store(true);
	thread = std::thread(&dv::Module::runThread, this);
}

dv::Module::~Module() {
	// Check module is properly shut down, which takes care of
	// cleaning up all input connections. This should always be
	// the case as it's a requirement for calling removeModule().
	if (isRunning) {
		dv::Log(dv::logLevel::CRITICAL, "Destroying a running module. This should never happen!");
	}

	// Stop module thread and wait for it to exit.
	threadAlive.store(false);
	runCond.notify_all();

	thread.join();

	// Now take care of cleaning up all output connections.
	for (auto &output : outputs) {
		std::scoped_lock lock(output.second.destinationsLock);

		for (auto &dest : output.second.destinations) {
			// Break link. Queue remains and just receives no new data.
			dest.linkedInput->source.linkedOutput = nullptr;

			// Downstream module has now an incorrect, impossible
			// input configuration. Let's stop it so the user can
			// fix it and restart it then.
			dvCfg::Node(dest.linkedInput->relatedModule->moduleNode).put<dvCfgType::BOOL>("running", false);
		}

		output.second.destinations.clear();
	}

	// Cleanup configuration and types.
	dvCfg::Node(moduleNode).removeNode();

	MainData::getGlobal().typeSystem.unregisterModuleTypes(this);

	// Last, unload the shared library plugin.
	dv::ModulesUnloadLibrary(library);
}

void dv::Module::LoggingInit() {
	auto moduleConfigNode = dvCfg::Node(moduleNode);

	// Per-module custom log string prefix.
	logger.logPrefix = name;

	// Per-module log level support. Initialize with global log level value.
	moduleConfigNode.create<dvCfgType::INT>("logLevel", caerLogLevelGet(), {CAER_LOG_EMERGENCY, CAER_LOG_DEBUG},
		dvCfgFlags::NORMAL, "Module-specific log-level.");

	logger.logLevel.store(moduleConfigNode.get<dvCfgType::INT>("logLevel"));
	moduleConfigNode.addAttributeListener(&logger.logLevel, &moduleLogLevelListener);

	// Switch to current module logger.
	dv::LoggerSet(&logger);
}

void dv::Module::RunningInit() {
	auto moduleConfigNode = dvCfg::Node(moduleNode);

	// Initialize shutdown controls. By default modules always run.
	// Allow for users to disable a module at start.
	moduleConfigNode.create<dvCfgType::BOOL>("autoStartup", true, {}, dvCfgFlags::NORMAL,
		"Start this module when the mainloop starts and keep retrying if initialization fails.");

	moduleConfigNode.create<dvCfgType::BOOL>(
		"running", false, {}, dvCfgFlags::NORMAL | dvCfgFlags::NO_EXPORT, "Module start/stop.");

	moduleConfigNode.create<dvCfgType::BOOL>(
		"isRunning", false, {}, dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Module running state.");

	bool runModule = moduleConfigNode.get<dvCfgType::BOOL>("autoStartup");

	running = runModule;
	moduleConfigNode.put<dvCfgType::BOOL>("running", runModule);

	isRunning = false;
	moduleConfigNode.updateReadOnly<dvCfgType::BOOL>("isRunning", false);

	moduleConfigNode.addAttributeListener(this, &moduleShutdownListener);
}

void dv::Module::StaticInit() {
	auto moduleConfigNode = dvCfg::Node(moduleNode);

	moduleConfigNode.addAttributeListener(&configUpdate, &moduleConfigUpdateListener);

	// Call module's staticInit function to create default static config.
	if (info->functions->moduleStaticInit != nullptr) {
		try {
			info->functions->moduleStaticInit(moduleNode);
		}
		catch (const std::exception &ex) {
			boost::format exMsg = boost::format("%s: moduleStaticInit() failed, error '%s'.") % name % ex.what();
			dv::Log(dv::logLevel::ERROR, exMsg);
			throw std::invalid_argument(exMsg.str().c_str());
		}
	}

	// Each module can set priority attributes for UI display. By default let's show 'running'.
	// Called last to allow for configInit() function to create a different default first.
	moduleConfigNode.attributeModifierPriorityAttributes("running");
}

void dv::Module::registerType(const dv::Types::Type type) {
	MainData::getGlobal().typeSystem.registerModuleType(this, type);
}

void dv::Module::registerInput(std::string_view inputName, std::string_view typeName, bool optional) {
	auto typeInfo = MainData::getGlobal().typeSystem.getTypeInfo(typeName, this);

	std::string inputNameString(inputName);

	if (inputs.count(inputNameString)) {
		throw std::invalid_argument("Input with name '" + inputNameString + "' already exists.");
	}

	// Add info to internal data structure.
	inputs.try_emplace(inputNameString, this, typeInfo, optional);

	// Add info to ConfigTree.
	auto moduleConfigNode = dvCfg::Node(moduleNode);

	auto inputNode = moduleConfigNode.getRelativeNode("inputs/" + inputNameString + "/");

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
	auto typeInfo = MainData::getGlobal().typeSystem.getTypeInfo(typeName, this);

	std::string outputNameString(outputName);

	if (outputs.count(outputNameString)) {
		throw std::invalid_argument("Output with name '" + outputNameString + "' already exists.");
	}

	// Add info to internal data structure.
	outputs.try_emplace(outputNameString, this, typeInfo);

	// Add info to ConfigTree.
	auto moduleConfigNode = dvCfg::Node(moduleNode);

	auto outputNode = moduleConfigNode.getRelativeNode("outputs/" + outputNameString + "/");

	outputNode.create<dvCfgType::STRING>("typeIdentifier", typeInfo.identifier, {4, 4},
		dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Type identifier.");
	outputNode.create<dvCfgType::STRING>("typeDescription", typeInfo.description, {1, 200},
		dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Type description.");
}

static const std::regex inputConnRegex("^([a-zA-Z-_\\d\\.]+)\\[([a-zA-Z-_\\d\\.]+)\\]$");

bool dv::Module::inputConnectivityInitialize() {
	for (auto &input : inputs) {
		// Get current module connectivity configuration.
		auto moduleConfigNode = dvCfg::Node(moduleNode);

		auto inputNode = moduleConfigNode.getRelativeNode("inputs/" + input.first + "/");
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
		if (!MainData::getGlobal().modules.count(moduleName)) {
			auto msg = boost::format("Input '%s': invalid connectivity attribute, module '%s' doesn't exist.")
					   % input.first % moduleName;
			dv::Log(dv::logLevel::ERROR, msg);
			return (false);
		}

		auto otherModule = MainData::getGlobal().modules[moduleName];

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
		OutgoingConnection connection{&input.second, input.second.source.queue};

		connectToModuleOutput(moduleOutput, connection);

		// And make that connection bidirectional.
		input.second.source.linkedOutput = moduleOutput;
	}

	return (true);
}

dv::ModuleOutput *dv::Module::getModuleOutput(const std::string &outputName) {
	try {
		return (&outputs.at(outputName));
	}
	catch (const std::out_of_range &) {
		// Fine, not found.
		return (nullptr);
	}
}

dv::ModuleInput *dv::Module::getModuleInput(const std::string &outputName) {
	try {
		return (&inputs.at(outputName));
	}
	catch (const std::out_of_range &) {
		// Fine, not found.
		return (nullptr);
	}
}

void dv::Module::connectToModuleOutput(ModuleOutput *output, OutgoingConnection connection) {
	std::scoped_lock lock(output->destinationsLock);

	output->destinations.push_back(connection);
}

void dv::Module::disconnectFromModuleOutput(ModuleOutput *output, OutgoingConnection connection) {
	std::scoped_lock lock(output->destinationsLock);

	auto pos = std::find(output->destinations.begin(), output->destinations.end(), connection);

	if (pos != output->destinations.end()) {
		output->destinations.erase(pos);
	}
}

void dv::Module::inputConnectivityDestroy() {
	// Cleanup inputs, disconnect from all of them.
	for (auto &input : inputs) {
		if (input.second.source.linkedOutput == nullptr) {
			// Not connected to another output, skip.
			continue;
		}

		// Remove the connection from the output.
		OutgoingConnection connection{&input.second, input.second.source.queue};

		disconnectFromModuleOutput(input.second.source.linkedOutput, connection);

		// Dissolve bond.
		input.second.source.linkedOutput = nullptr;

		// Empty queue of any remaining data elements.
		{
			std::scoped_lock lock(dataLock);

			while (!input.second.source.queue->empty()) {
				auto dataPtr = input.second.source.queue->get();

				// Ensure refcount is decremented properly.
				boost::intrusive_ptr<IntrusiveTypedObject> packet(dataPtr, false);

				dataAvailable--;
			}
		}

		// Empty per-input tracker of live memory of remaining data.
		input.second.inUsePackets.clear();
	}
}

void dv::Module::handleModuleInitFailure() {
	// Disconnect from other modules.
	inputConnectivityDestroy();

	auto moduleConfigNode = dvCfg::Node(moduleNode);

	// Set running back to false on initialization failure.
	moduleConfigNode.put<dvCfgType::BOOL>("running", false);

	// Schedule retry on next update-handler pass, if module should
	// automatically retry starting up and initializing.
	if (moduleConfigNode.get<dvCfgType::BOOL>("autoStartup")) {
		moduleConfigNode.attributeUpdaterAdd("running", dvCfgType::BOOL,
			[](void *, const char *, enum dvConfigAttributeType) {
				dvConfigAttributeValue val;
				val.boolean = true;
				return (val);
			},
			nullptr, true);
	}
}

void dv::Module::runThread() {
	// Set thread-local logger once at startup.
	dv::LoggerSet(&logger);

	while (threadAlive.load(std::memory_order_relaxed)) {
		runStateMachine();
	}
}

void dv::Module::runStateMachine() {
	auto moduleConfigNode = dvCfg::Node(moduleNode);

	bool shouldRun = false;

	{
		std::unique_lock lock(runLock);

		runCond.wait(lock, [this]() {
			if (!threadAlive.load(std::memory_order_relaxed)) {
				return (true); // Stop waiting on thread exit.
			}

			return (running || isRunning);
		});

		shouldRun = running;
	}

	if (isRunning && shouldRun) {
		if (configUpdate.load(std::memory_order_relaxed)) {
			configUpdate.store(false);

			if (info->functions->moduleConfig != nullptr) {
				// Call config function. 'configUpdate' variable reset is done above.
				try {
					info->functions->moduleConfig(this);
				}
				catch (const std::exception &ex) {
					dv::Log(dv::logLevel::ERROR, "moduleConfig(): '%s', disabling module.", ex.what());

					moduleConfigNode.put<dvCfgType::BOOL>("running", false);
					return;
				}
			}
		}

		// Only run if there is data. On timeout with no data, do nothing.
		// If is an input generation module (no inputs defined at all), always run.
		if (inputs.size() > 0) {
			std::unique_lock lock(dataLock);

			if (!dataCond.wait_for(lock, std::chrono::seconds(1), [this]() { return (dataAvailable > 0); })) {
				return;
			}
		}

		if (info->functions->moduleRun != nullptr) {
			try {
				info->functions->moduleRun(this);
			}
			catch (const std::exception &ex) {
				dv::Log(dv::logLevel::ERROR, "moduleRun(): '%s', disabling module.", ex.what());

				moduleConfigNode.put<dvCfgType::BOOL>("running", false);
				return;
			}
		}
	}
	else if (!isRunning && shouldRun) {
		// Serialize module start/stop globally.
		std::scoped_lock lock(MainData::getGlobal().modulesLock);

		// At module startup, first check that input connectivity is
		// satisfied and hook up the input queues.
		if (!inputConnectivityInitialize()) {
			dv::Log(dv::logLevel::ERROR, "moduleInit(): '%s', disabling module.", "input connectivity failure");

			handleModuleInitFailure();
			return;
		}

		// Allocate memory for module state.
		if (info->memSize != 0) {
			moduleState = calloc(1, info->memSize);
			if (moduleState == nullptr) {
				dv::Log(dv::logLevel::ERROR, "moduleInit(): '%s', disabling module.", "memory allocation failure");

				handleModuleInitFailure();
				return;
			}
		}
		else {
			// memSize is zero, so moduleState must be nullptr.
			moduleState = nullptr;
		}

		// Reset variables, as the following Init() is stronger than a reset
		// and implies a full configuration update. This avoids stale state
		// forcing an update and/or reset right away in the first run of
		// the module, which is unneeded and wasteful.
		configUpdate.store(false);

		if (info->functions->moduleInit != nullptr) {
			try {
				if (!info->functions->moduleInit(this)) {
					throw std::runtime_error("Failed to initialize module.");
				}
			}
			catch (const std::exception &ex) {
				dv::Log(dv::logLevel::ERROR, "moduleInit(): '%s', disabling module.", ex.what());

				if (info->memSize != 0) {
					// Only deallocate if we were the original allocator.
					free(moduleState);
				}
				moduleState = nullptr;

				handleModuleInitFailure();
				return;
			}
		}

		isRunning = true;
		moduleConfigNode.updateReadOnly<dvCfgType::BOOL>("isRunning", true);
	}
	else if (isRunning && !shouldRun) {
		// Serialize module start/stop globally.
		std::scoped_lock lock(MainData::getGlobal().modulesLock);

		if (info->functions->moduleExit != nullptr) {
			try {
				info->functions->moduleExit(this);
			}
			catch (const std::exception &ex) {
				dv::Log(dv::logLevel::ERROR, "moduleExit(): '%s'.", ex.what());
			}
		}

		if (info->memSize != 0) {
			// Only deallocate if we were the original allocator.
			free(moduleState);
		}
		moduleState = nullptr;

		// Disconnect from other modules.
		inputConnectivityDestroy();

		isRunning = false;
		moduleConfigNode.updateReadOnly<dvCfgType::BOOL>("isRunning", false);
	}
}

dv::Types::TypedObject *dv::Module::outputAllocate(std::string_view outputName) {
	auto output = getModuleOutput(std::string(outputName));
	if (output == nullptr) {
		// Not found.
		auto msg = boost::format("Output with name '%s' doesn't exist.") % outputName;
		throw std::out_of_range(msg.str());
	}

	if (!output->nextPacket) {
		// Allocate new and store.
		output->nextPacket = new IntrusiveTypedObject(output->type);
	}

	// Return current value.
	return (output->nextPacket.get());
}

void dv::Module::outputCommit(std::string_view outputName) {
	auto output = getModuleOutput(std::string(outputName));
	if (output == nullptr) {
		// Not found.
		auto msg = boost::format("Output with name '%s' doesn't exist.") % outputName;
		throw std::out_of_range(msg.str());
	}

	if (!output->nextPacket) {
		// Not previously allocated, ignore.
		return;
	}

	{
		std::scoped_lock lock(output->destinationsLock);

		for (auto &dest : output->destinations) {
			// Send new data to downstream module, increasing its reference
			// count to share ownership amongst the downstream modules.
			auto refInc = output->nextPacket;

			try {
				dest.queue->put(refInc.get());
				refInc.detach();
			}
			catch (const std::out_of_range &) {
				// TODO: notify user somehow, statistics?
				continue;
			}

			// Notify downstream module about new data being available.
			{
				std::scoped_lock lock2(dest.linkedInput->relatedModule->dataLock);
				dest.linkedInput->relatedModule->dataAvailable++;
			}

			dest.linkedInput->relatedModule->dataCond.notify_all();
		}
	}

	output->nextPacket.reset();
}

const dv::Types::TypedObject *dv::Module::inputGet(std::string_view inputName) {
	auto input = getModuleInput(std::string(inputName));
	if (input == nullptr) {
		// Not found.
		auto msg = boost::format("Input with name '%s' doesn't exist.") % inputName;
		throw std::out_of_range(msg.str());
	}

	IntrusiveTypedObject *dataPtr = nullptr;

	try {
		dataPtr = input->source.queue->get();
	}
	catch (const std::out_of_range &) {
		// Empty queue, no data, return NULL.
		return (nullptr);
	}

	{
		std::scoped_lock lock(dataLock);
		dataAvailable--;
	}

	boost::intrusive_ptr<IntrusiveTypedObject> nextPacket(dataPtr, false);

	input->inUsePackets.push_back(nextPacket);

	return (nextPacket.get());
}

void dv::Module::inputDismiss(std::string_view inputName, const dv::Types::TypedObject *data) {
	auto input = getModuleInput(std::string(inputName));
	if (input == nullptr) {
		// Not found.
		auto msg = boost::format("Input with name '%s' doesn't exist.") % inputName;
		throw std::out_of_range(msg.str());
	}

	auto pos = std::find(input->inUsePackets.begin(), input->inUsePackets.end(), data);

	if (pos != input->inUsePackets.end()) {
		input->inUsePackets.erase(pos);
	}
}

void dv::Module::moduleShutdownListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	auto module = static_cast<dv::Module *>(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_BOOL && caerStrEquals(changeKey, "running")) {
		{
			std::scoped_lock lock(module->runLock);

			module->running = changeValue.boolean;
		}

		module->runCond.notify_all();
	}
}

void dv::Module::moduleLogLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);

	auto logLevel = static_cast<std::atomic_int32_t *>(userData);

	if (event == DVCFG_ATTRIBUTE_MODIFIED && changeType == DVCFG_TYPE_INT && caerStrEquals(changeKey, "logLevel")) {
		logLevel->store(changeValue.iint);
	}
}

void dv::Module::moduleConfigUpdateListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue) {
	UNUSED_ARGUMENT(node);
	UNUSED_ARGUMENT(changeKey);
	UNUSED_ARGUMENT(changeType);
	UNUSED_ARGUMENT(changeValue);

	auto configUpdate = static_cast<std::atomic_bool *>(userData);

	// Simply set the config update flag to 1 on any attribute change.
	if (event == DVCFG_ATTRIBUTE_MODIFIED) {
		configUpdate->store(true);
	}
}
