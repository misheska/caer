#include "module.hpp"

#include "dv-sdk/cross/portable_io.h"
#include "dv-sdk/cross/portable_threads.h"

#include "main.hpp"

#include <algorithm>
#include <boost/core/demangle.hpp>
#include <boost/format.hpp>
#include <chrono>
#include <regex>
#include <typeinfo>

namespace dvCfg  = dv::Config;
using dvCfgType  = dvCfg::AttributeType;
using dvCfgFlags = dvCfg::AttributeFlags;

std::atomic_uint64_t dv::Module::controlIDGenerator{0};

dv::Module::Module(std::string_view _name, std::string_view _library) :
	name(_name),
	running(false),
	isRunning(false),
	configUpdate(false),
	threadAlive(false) {
	// Load library to get module functions.
	try {
		std::tie(library, info) = dv::ModulesLoadLibrary(_library);
	}
	catch (const std::exception &ex) {
		auto exMsg = boost::format("%s: module library load failed, error '%s'.") % name % ex.what();
		dv::Log(dv::logLevel::ERROR, exMsg);
		throw std::invalid_argument(exMsg.str());
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

	dv::Log(dv::logLevel::DEBUG, "%s", "Module initialized.");

	// Start module thread.
	threadAlive.store(true);
	thread = std::thread(&dv::Module::runThread, this);
}

dv::Module::~Module() {
	// Switch to current module logger.
	dv::LoggerSet(&logger);

	// Check module is properly shut down, which takes care of
	// cleaning up all input connections. This should always be
	// the case as it's a requirement for calling removeModule().
	if (isRunning) {
		dv::Log(dv::logLevel::CRITICAL, "%s", "Destroying a running module. This should never happen!");
	}

	// Stop module thread and wait for it to exit.
	threadAlive.store(false);
	runCond.notify_all();

	thread.join();

	// Now take care of notifying all downstream modules.
	{
		std::scoped_lock lock(controlLock);

		for (auto &dest : controlDestinations) {
			// Downstream module has now an incorrect, impossible
			// input configuration. Let's stop it so the user can
			// fix it and restart it then.
			// TODO: use control system.
		}

		controlDestinations.clear();
	}

	// Cleanup configuration and types.
	dvCfg::Node(moduleNode).removeNode();

	MainData::getGlobal().typeSystem.unregisterModuleTypes(this);

	dv::Log(dv::logLevel::DEBUG, "%s", "Module destroyed.");

	// Last, unload the shared library plugin.
	dv::ModulesUnloadLibrary(library);
}

void dv::Module::LoggingInit() {
	auto moduleConfigNode = dvCfg::Node(moduleNode);

	// Per-module custom log string prefix.
	logger.logPrefix = name;

	// Per-module log level support. Initialize with global log level value.
	moduleConfigNode.create<dvCfgType::INT>("logLevel", CAER_LOG_NOTICE, {CAER_LOG_EMERGENCY, CAER_LOG_DEBUG},
		dvCfgFlags::NORMAL, "Module-specific log-level.");

	moduleConfigNode.addAttributeListener(&logger.logLevel, &moduleLogLevelListener);
	logger.logLevel.store(moduleConfigNode.get<dvCfgType::INT>("logLevel"));

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
			info->functions->moduleStaticInit(this);
		}
		catch (const std::exception &ex) {
			auto exMsg = boost::format("%s: moduleStaticInit() failed, error '%s'.") % name % ex.what();
			dv::Log(dv::logLevel::ERROR, exMsg);
			throw std::invalid_argument(exMsg.str());
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
	inputs.try_emplace(inputNameString, typeInfo, optional);

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

	dv::Log(dv::logLevel::DEBUG, "Input '%s' registered with type '%s' (optional=%d).", inputNameString.c_str(),
		typeInfo.identifier, optional);
}

void dv::Module::registerOutput(std::string_view outputName, std::string_view typeName) {
	auto typeInfo = MainData::getGlobal().typeSystem.getTypeInfo(typeName, this);

	std::string outputNameString(outputName);

	if (outputs.count(outputNameString)) {
		throw std::invalid_argument("Output with name '" + outputNameString + "' already exists.");
	}

	// Add info to ConfigTree.
	auto moduleConfigNode = dvCfg::Node(moduleNode);

	auto outputNode = moduleConfigNode.getRelativeNode("outputs/" + outputNameString + "/");

	outputNode.create<dvCfgType::STRING>("typeIdentifier", typeInfo.identifier, {4, 4},
		dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Type identifier.");
	outputNode.create<dvCfgType::STRING>("typeDescription", typeInfo.description, {1, 200},
		dvCfgFlags::READ_ONLY | dvCfgFlags::NO_EXPORT, "Type description.");

	auto infoNode = outputNode.getRelativeNode("info/");

	// Add info to internal data structure.
	outputs.try_emplace(outputNameString, typeInfo, infoNode);

	dv::Log(
		dv::logLevel::DEBUG, "Output '%s' registered with type '%s'.", outputNameString.c_str(), typeInfo.identifier);
}

static const std::regex inputConnRegex("^([a-zA-Z-_\\d\\.]+)\\[([a-zA-Z-_\\d\\.]+)\\]$");

std::pair<std::string, std::string> dv::Module::parseModuleInputString(const std::string &moduleInput) {
	// Not empty, so check syntax and then components.
	std::smatch inputConnComponents;
	if (!std::regex_match(moduleInput, inputConnComponents, inputConnRegex)) {
		throw std::out_of_range("Invalid format of connectivity attribute '" + moduleInput + "'.");
	}

	auto moduleName = inputConnComponents.str(1);
	auto outputName = inputConnComponents.str(2);

	return (std::make_pair(moduleName, outputName));
}

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

		std::string moduleName, outputName;

		try {
			std::tie(moduleName, outputName) = parseModuleInputString(inputConn);
		}
		catch (const std::out_of_range &ex) {
			auto msg = boost::format("Input '%s': %s") % input.first % ex.what();
			dv::Log(dv::logLevel::ERROR, msg);
			return (false);
		}

		// Does the referenced module exist?
		auto otherModule = getModule(moduleName);
		if (otherModule == nullptr) {
			auto msg = boost::format("Input '%s': invalid connectivity attribute, module '%s' doesn't exist.")
					   % input.first % moduleName;
			dv::Log(dv::logLevel::ERROR, msg);
			return (false);
		}

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
		// If the expected type is ANYT, we accept anything.
		if ((input.second.type.id != dv::Types::anyId) && (input.second.type.id != moduleOutput->type.id)) {
			auto msg = boost::format("Input '%s': invalid connectivity attribute, output '%s' in module '%s' has type "
									 "'%s', but this input requires type '%s'.")
					   % input.first % outputName % moduleName % moduleOutput->type.identifier % input.second.type.id;
			dv::Log(dv::logLevel::ERROR, msg);
			return (false);
		}

		// All is well, let's connect to that output.
		OutConnection connection{&input.second.queue, &dataAvailable};

		connectToModuleOutput(moduleOutput, connection);

		// And we're done.
		input.second.linkedOutput = inputConn;
	}

	return (true);
}

dv::Module *dv::Module::getModule(const std::string &moduleName) {
	try {
		return (MainData::getGlobal().modules.at(moduleName).get());
	}
	catch (const std::out_of_range &) {
		// Fine, not found.
		return (nullptr);
	}
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

void dv::Module::connectToModuleOutput(ModuleOutput *output, OutConnection connection) {
	std::scoped_lock lock(output->destinationsLock);

	output->destinations.push_back(connection);
}

void dv::Module::disconnectFromModuleOutput(ModuleOutput *output, OutConnection connection) {
	std::scoped_lock lock(output->destinationsLock);

	auto pos = std::find(output->destinations.begin(), output->destinations.end(), connection);

	if (pos != output->destinations.end()) {
		output->destinations.erase(pos);
	}
}

void dv::Module::inputConnectivityDestroy() {
	// Cleanup inputs, disconnect from all of them.
	for (auto &input : inputs) {
		if (!input.second.linkedOutput.empty()) {
			const auto [moduleName, outputName] = parseModuleInputString(input.second.linkedOutput);

			auto otherModule = getModule(moduleName);

			auto moduleOutput = otherModule->getModuleOutput(outputName);

			// Remove the connection from the output.
			OutConnection connection{&input.second.queue, &dataAvailable};

			disconnectFromModuleOutput(moduleOutput, connection);

			// Disconnected.
			input.second.linkedOutput.clear();
		}

		// Empty queue of any remaining data elements.
		{
			std::scoped_lock lock(dataAvailable.lock);

			while (!input.second.queue.empty()) {
				auto dataPtr = input.second.queue.get();

				// Ensure refcount is decremented properly.
				boost::intrusive_ptr<IntrusiveTypedObject> packet(dataPtr, false);

				dataAvailable.count--;
			}
		}

		// Empty per-input tracker of live memory of remaining data.
		input.second.inUsePackets.clear();
	}
}

/**
 * Check that each output's info node has been populated with
 * at least one informative attribute, so that downstream modules
 * can find out relevant information about the output.
 *
 * @return True if all outputs have info attributes, false otherwise.
 */
bool dv::Module::verifyOutputInfoNodes() {
	for (const auto &out : outputs) {
		if (out.second.infoNode.getAttributeKeys().size() == 0) {
			auto msg = boost::format("Output '%s' has no informative attributes in info node.") % out.first;
			dv::Log(dv::logLevel::ERROR, msg);

			return (false);
		}
	}

	return (true);
}

/**
 * Remove all attributes and children of the informative
 * output nodes, leaving them empty.
 * This cleanup should happen on module initialization failure
 * and on module exit, to ensure no stale attributes are kept.
 */
void dv::Module::cleanupOutputInfoNodes() {
	for (auto &out : outputs) {
		out.second.infoNode.clearSubTree(true);
		out.second.infoNode.removeSubTree();
	}
}

void dv::Module::shutdownProcedure(bool doModuleExit) {
	if (doModuleExit && info->functions->moduleExit != nullptr) {
		try {
			info->functions->moduleExit(this);
		}
		catch (const std::exception &ex) {
			dv::Log(dv::logLevel::ERROR, "moduleExit(): '%s'.", ex.what());
		}
	}

	// Free state memory.
	if (info->memSize != 0) {
		// Only deallocate if we were the original allocator.
		free(moduleState);
	}
	moduleState = nullptr;

	// Disconnect from other modules.
	inputConnectivityDestroy();

	// Cleanup output info nodes, if any exist.
	cleanupOutputInfoNodes();
}

void dv::Module::handleModuleInitFailure(bool doModuleExit) {
	shutdownProcedure(doModuleExit);

	auto moduleConfigNode = dvCfg::Node(moduleNode);

	// Set running back to false on initialization failure.
	moduleConfigNode.put<dvCfgType::BOOL>("running", false);

	// Schedule retry on next update-handler pass, if module should
	// automatically retry starting up and initializing.
	if (moduleConfigNode.get<dvCfgType::BOOL>("autoStartup")) {
		moduleConfigNode.attributeUpdaterAdd(
			"running", dvCfgType::BOOL,
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

	// Set thread name.
	portable_thread_set_name(logger.logPrefix.c_str());

	dv::Log(dv::logLevel::DEBUG, "%s", "Module thread running.");

	// Run state machine as long as module is running.
	while (threadAlive.load(std::memory_order_relaxed)) {
		runStateMachine();
	}

	dv::Log(dv::logLevel::DEBUG, "%s", "Module thread stopped.");
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
					dv::Log(dv::logLevel::ERROR, "moduleConfig(): '%s :: %s', disabling module.",
						boost::core::demangle(typeid(ex).name()).c_str(), ex.what());

					moduleConfigNode.put<dvCfgType::BOOL>("running", false);
					return;
				}
			}
		}

		// Only run if there is data. On timeout with no data, do nothing.
		// If is an input generation module (no inputs defined at all), always run.
		if (inputs.size() > 0) {
			std::unique_lock lock(dataAvailable.lock);

			if (!dataAvailable.cond.wait_for(
					lock, std::chrono::seconds(1), [this]() { return (dataAvailable.count > 0); })) {
				return;
			}
		}

		if (info->functions->moduleRun != nullptr) {
			try {
				info->functions->moduleRun(this);
			}
			catch (const std::exception &ex) {
				dv::Log(dv::logLevel::ERROR, "moduleRun(): '%s :: %s', disabling module.",
					boost::core::demangle(typeid(ex).name()).c_str(), ex.what());

				moduleConfigNode.put<dvCfgType::BOOL>("running", false);
				return;
			}
		}
	}
	else if (!isRunning && shouldRun) {
		// Serialize module start/stop globally.
		std::scoped_lock lock(MainData::getGlobal().modulesLock);

		// Allocate memory for module state.
		if (info->memSize != 0) {
			moduleState = calloc(1, info->memSize);
			if (moduleState == nullptr) {
				dv::Log(dv::logLevel::ERROR, "moduleInit(): '%s', disabling module.", "memory allocation failure");

				handleModuleInitFailure(false);
				return;
			}
		}
		else {
			// memSize is zero, so moduleState must be nullptr.
			moduleState = nullptr;
		}

		// At module startup, check that input connectivity is
		// satisfied and hook up the input queues.
		if (!inputConnectivityInitialize()) {
			dv::Log(dv::logLevel::ERROR, "moduleInit(): '%s', disabling module.", "input connectivity failure");

			handleModuleInitFailure(false);
			return;
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
				dv::Log(dv::logLevel::ERROR, "moduleInit(): '%s :: %s', disabling module.",
					boost::core::demangle(typeid(ex).name()).c_str(), ex.what());

				handleModuleInitFailure(false);
				return;
			}
		}

		// Check that all info nodes for the outputs have been created and populated.
		if (!verifyOutputInfoNodes()) {
			dv::Log(dv::logLevel::ERROR, "moduleInit(): '%s', disabling module.", "incomplete ouput information");

			handleModuleInitFailure(true);
			return;
		}

		isRunning = true;
		moduleConfigNode.updateReadOnly<dvCfgType::BOOL>("isRunning", true);
	}
	else if (isRunning && !shouldRun) {
		// Serialize module start/stop globally.
		std::scoped_lock lock(MainData::getGlobal().modulesLock);

		// Full shutdown.
		shutdownProcedure(true);

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
				std::scoped_lock lock2(dest.dataAvailable->lock);
				dest.dataAvailable->count++;
			}

			dest.dataAvailable->cond.notify_all();
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
		dataPtr = input->queue.get();
	}
	catch (const std::out_of_range &) {
		// Empty queue, no data, return NULL.
		return (nullptr);
	}

	{
		std::scoped_lock lock(dataAvailable.lock);
		dataAvailable.count--;
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

/**
 * Get informative node for an output of this module.
 * Can always be called.
 *
 * @param outputName name of output.
 * @return informative node for that output.
 */
dv::Config::Node dv::Module::outputGetInfoNode(std::string_view outputName) {
	auto output = getModuleOutput(std::string(outputName));
	if (output == nullptr) {
		// Not found.
		auto msg = boost::format("Output with name '%s' doesn't exist.") % outputName;
		throw std::out_of_range(msg.str());
	}

	return (output->infoNode);
}

/**
 * Get informative node for an input from that input's upstream module.
 * Can only be called while global modules lock is held, ie. from moduleStaticInit(),
 * moduleInit() and moduleExit(). Do not hold references in state!
 *
 * @param inputName name of input.
 * @return informative node for that input.
 */
const dv::Config::Node dv::Module::inputGetInfoNode(std::string_view inputName) {
#ifndef NDEBUG
	if (isRunning) {
		auto msg
			= boost::format("Function '%s' cannot be called outside of StaticInit(), Init() and Exit().") % __func__;
		throw std::out_of_range(msg.str());
	}
#endif

	auto input = getModuleInput(std::string(inputName));
	if (input == nullptr) {
		// Not found.
		auto msg = boost::format("Input with name '%s' doesn't exist.") % inputName;
		throw std::out_of_range(msg.str());
	}

	auto outputLink = input->linkedOutput;
	if (outputLink.empty()) {
		// Input can be unconnected.
		auto msg = boost::format("Input '%s' is unconnected.") % inputName;
		throw std::runtime_error(msg.str());
	}

	const auto [moduleName, outputName] = parseModuleInputString(outputLink);

	auto otherModule = getModule(moduleName);

	auto moduleOutput = otherModule->getModuleOutput(outputName);

	auto infoNode = moduleOutput->infoNode;
	if (infoNode.getAttributeKeys().size() == 0) {
		auto msg = boost::format("No informative content present for input '%s'.") % inputName;
		throw std::runtime_error(msg.str());
	}

	return (infoNode);
}

/**
 * Check if an input is connected properly and can get data at runtime.
 * Can always be called. Most useful in moduleInit() to verify status
 * of optional inputs connections.
 *
 * @param inputName name of input.
 * @return true if input is connected and valid, false otherwise.
 */
bool dv::Module::inputIsConnected(std::string_view inputName) {
	auto input = getModuleInput(std::string(inputName));
	if (input == nullptr) {
		// Not found.
		auto msg = boost::format("Input with name '%s' doesn't exist.") % inputName;
		throw std::out_of_range(msg.str());
	}

	return (!input->linkedOutput.empty());
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
