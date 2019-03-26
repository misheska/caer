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

class IncomingConnection {
public:
	ModuleOutput *linkedOutput;
	libcaer::ringbuffer::RingBuffer queue;

	IncomingConnection(ModuleOutput *from) : linkedOutput(from), queue(INTER_MODULE_TRANSFER_QUEUE_SIZE) {
	}

	bool operator==(const IncomingConnection &rhs) const noexcept {
		return ((linkedOutput == rhs.linkedOutput) && (queue == rhs.queue));
	}

	bool operator!=(const IncomingConnection &rhs) const noexcept {
		return (!operator==(rhs));
	}
};

class ModuleInput {
public:
	Module *relatedModule;
	dv::Types::Type type;
	bool optional;
	IncomingConnection source;

	ModuleInput(Module *parent, const dv::Types::Type &t, bool opt) :
		relatedModule(parent),
		type(t),
		optional(opt),
		source(nullptr) {
	}
};

class OutgoingConnection {
public:
	ModuleInput *linkedInput;
	libcaer::ringbuffer::RingBuffer queue;

	OutgoingConnection(ModuleInput *to, libcaer::ringbuffer::RingBuffer linkedQueue) :
		linkedInput(to),
		queue(linkedQueue) {
	}

	bool operator==(const OutgoingConnection &rhs) const noexcept {
		return ((linkedInput == rhs.linkedInput) && (queue == rhs.queue));
	}

	bool operator!=(const OutgoingConnection &rhs) const noexcept {
		return (!operator==(rhs));
	}
};

class ModuleOutput {
public:
	Module *relatedModule;
	dv::Types::Type type;
	std::mutex destinationsLock;
	std::vector<OutgoingConnection> destinations;

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
	std::unordered_map<std::string, ModuleInput> inputs;
	std::unordered_map<std::string, ModuleOutput> outputs;
	dv::Config::Node moduleNode; // C++ convenience variant.
	dvModuleDataS data;

public:
	Module(std::string_view _name, std::string_view _library);
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
	ModuleInput *getModuleInput(const std::string &outputName);

	static void connectToModuleOutput(ModuleOutput *output, OutgoingConnection connection);
	static void disconnectFromModuleOutput(ModuleOutput *output, OutgoingConnection connection);

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
