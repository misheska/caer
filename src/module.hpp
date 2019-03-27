#ifndef MODULE_HPP_
#define MODULE_HPP_

#include <libcaercpp/ringbuffer.hpp>

#include "dv-sdk/module.h"

#include "log.hpp"
#include "mainloop.h"
#include "modules_discovery.hpp"
#include "types.hpp"

#include <atomic>
#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#define INTER_MODULE_TRANSFER_QUEUE_SIZE 256

namespace dv {

class ModuleInput;
class ModuleOutput;

class IntrusiveTypedObject : public dv::Types::TypedObject,
							 public boost::intrusive_ref_counter<IntrusiveTypedObject, boost::thread_safe_counter> {
public:
	IntrusiveTypedObject(const dv::Types::Type &t) : dv::Types::TypedObject(t) {
	}
};

using ModuleBuffer = libcaer::ringbuffer::RingBuffer<IntrusiveTypedObject *>;

// Module-related definitions.
enum class ModuleStatus {
	STOPPED = 0,
	RUNNING = 1,
};

class IncomingConnection {
public:
	ModuleOutput *linkedOutput;
	std::shared_ptr<ModuleBuffer> queue;

	IncomingConnection(ModuleOutput *from) :
		linkedOutput(from),
		queue(std::make_shared<ModuleBuffer>(INTER_MODULE_TRANSFER_QUEUE_SIZE)) {
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
	std::shared_ptr<ModuleBuffer> queue;

	OutgoingConnection(ModuleInput *to, std::shared_ptr<ModuleBuffer> linkedQueue) :
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
	boost::intrusive_ptr<IntrusiveTypedObject> nextPacket;

	ModuleOutput(Module *parent, const dv::Types::Type &t) : relatedModule(parent), type(t) {
	}
};

class Module : public dvModuleDataS {
private:
	// Module info.
	std::string name;
	dvModuleInfo info;
	dv::ModuleLibrary library;
	// Run status.
	ModuleStatus moduleStatus;
	std::atomic_bool running;
	std::atomic_bool configUpdate;
	// Logging.
	dv::LogBlock logger;
	// I/O connectivity.
	std::unordered_map<std::string, ModuleInput> inputs;
	std::unordered_map<std::string, ModuleOutput> outputs;

public:
	Module(std::string_view _name, std::string_view _library);
	~Module();

	void registerType(const dv::Types::Type type);
	void registerOutput(std::string_view name, std::string_view typeName);
	void registerInput(std::string_view name, std::string_view typeName, bool optional = false);

	void runStateMachine();

	dv::Types::TypedObject *outputAllocate(std::string_view outputName);
	void outputCommit(std::string_view outputName);
	const dv::Types::TypedObject *inputGet(std::string_view inputName);
	void inputDismiss(std::string_view inputName, const dv::Types::TypedObject *data);

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
