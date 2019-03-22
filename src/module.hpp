#ifndef MODULE_HPP_
#define MODULE_HPP_

#include <libcaercpp/ringbuffer.hpp>

#include "dv-sdk/module.h"

#include "log.hpp"
#include "mainloop.h"
#include "modules_discovery.hpp"
#include "types.hpp"

#include <atomic>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#define INTER_MODULE_TRANSFER_QUEUE_SIZE 256

namespace dv {

class ModuleInput;
class ModuleOutput;

// Module-related definitions.
enum class ModuleStatus {
	STOPPED = 0,
	RUNNING = 1,
};

class ModuleInput {
public:
	Module *relatedModule;
	dv::Types::Type type;
	bool optional;
	std::pair<ModuleOutput *, libcaer::ringbuffer::RingBuffer> queue;

	ModuleInput(Module *parent, const dv::Types::Type &t, bool opt) :
		relatedModule(parent),
		type(t),
		optional(opt),
		queue(std::make_pair(nullptr, INTER_MODULE_TRANSFER_QUEUE_SIZE)) {
	}
};

class ModuleOutput {
public:
	Module *relatedModule;
	dv::Types::Type type;
	std::mutex destinationsLock;
	std::vector<std::pair<ModuleInput *, libcaer::ringbuffer::RingBuffer>> destinations;

	ModuleOutput(Module *parent, const dv::Types::Type &t) : relatedModule(parent), type(t) {
	}
};

class Module {
private:
	std::string name;
	dvModuleInfo info;
	dv::ModuleLibrary library;
	ModuleStatus moduleStatus;
	std::atomic_bool running;
	std::atomic_uint32_t configUpdate;
	dv::LogBlock logger;
	dv::MainData *mainData;
	std::unordered_map<std::string, ModuleInput> inputs;
	std::unordered_map<std::string, ModuleOutput> outputs;
	dv::Config::Node moduleNode; // C++ convenience variant.
	dvModuleDataS data;

public:
	Module(std::string_view _name, std::string_view _library, dv::MainData *_mainData);
	~Module();

	dv::Config::Node getConfigNode();

	void registerType(const dv::Types::Type type);
	void registerOutput(std::string_view name, std::string_view typeName);
	void registerInput(std::string_view name, std::string_view typeName, bool optional = false);

	void runStateMachine();

private:
	void LoggingInit();
	void RunningInit();
	void StaticInit();

	ModuleOutput *getModuleOutput(const std::string &outputName);
	void connectToModuleOutput(
		ModuleOutput *output, std::pair<ModuleInput *, libcaer::ringbuffer::RingBuffer> &destinationQueue);
	void disconnectFromModuleOutput(
		ModuleOutput *output, std::pair<ModuleInput *, libcaer::ringbuffer::RingBuffer> &destinationQueue);

	bool inputConnectivityInitialize();
	void inputConnectivityDestroy();

	void handleModuleInitFailure();

	static void moduleShutdownListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
	static void moduleLogLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
	static void moduleConfigUpdateListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
};

} // namespace dv

#endif /* MODULE_HPP_ */
