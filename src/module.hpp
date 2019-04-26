#ifndef MODULE_HPP_
#define MODULE_HPP_

#include <libcaercpp/ringbuffer.hpp>

#include "dv-sdk/module.h"

#include "log.hpp"
#include "modules_discovery.hpp"

#include <atomic>
#include <boost/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <condition_variable>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>

#define INTER_MODULE_TRANSFER_QUEUE_SIZE 256

namespace dv {

class Module;
class ModuleInput;
class ModuleOutput;

class IntrusiveTypedObject : public dv::Types::TypedObject,
							 public boost::intrusive_ref_counter<IntrusiveTypedObject, boost::thread_safe_counter> {
public:
	IntrusiveTypedObject(const dv::Types::Type &t) : dv::Types::TypedObject(t) {
	}
};

class ModuleInput {
public:
	dv::Types::Type type;
	bool optional;
	ModuleOutput *linkedOutput;
	Module *parentModule;
	libcaer::ringbuffer::RingBuffer<IntrusiveTypedObject *> queue;
	std::vector<boost::intrusive_ptr<IntrusiveTypedObject>> inUsePackets;

	ModuleInput(const dv::Types::Type &t, bool opt, Module *parentModule_) :
		type(t),
		optional(opt),
		linkedOutput(nullptr),
		parentModule(parentModule_),
		queue(INTER_MODULE_TRANSFER_QUEUE_SIZE) {
	}
};

struct InputDataAvailable {
public:
	// Input data availability.
	std::mutex lock;
	std::condition_variable cond;
	int32_t count;

	InputDataAvailable() : count(0) {
	}
};

struct RunControl {
	// Run status.
	std::mutex lock;
	std::condition_variable cond;
	bool forcedShutdown;
	bool running;
	bool isRunning;
	std::atomic_bool configUpdate;

	RunControl() : forcedShutdown(false), running(false), isRunning(false), configUpdate(false) {
	}
};

class OutConnection {
public:
	libcaer::ringbuffer::RingBuffer<IntrusiveTypedObject *> *queue;
	InputDataAvailable *dataAvailable;
	RunControl *run;
	ModuleInput *linkedInput;

	OutConnection(libcaer::ringbuffer::RingBuffer<IntrusiveTypedObject *> *queue_, InputDataAvailable *dataAvailable_,
		RunControl *run_, ModuleInput *linkedInput_) :
		queue(queue_),
		dataAvailable(dataAvailable_),
		run(run_),
		linkedInput(linkedInput_) {
	}

	bool operator==(const OutConnection &rhs) const noexcept {
		// The linked input pointer is unique, so enough to establish equality.
		return (linkedInput == rhs.linkedInput);
	}

	bool operator!=(const OutConnection &rhs) const noexcept {
		return (!operator==(rhs));
	}
};

class ModuleOutput {
public:
	dv::Types::Type type;
	dv::Config::Node infoNode;
	Module *parentModule;
	std::mutex destinationsLock;
	std::vector<OutConnection> destinations;
	boost::intrusive_ptr<IntrusiveTypedObject> nextPacket;

	ModuleOutput(const dv::Types::Type &type_, dv::Config::Node infoNode_, Module *parentModule_) :
		type(type_),
		infoNode(infoNode_),
		parentModule(parentModule_) {
	}
};

class Module : public dvModuleDataS {
private:
	// Module info.
	std::string name;
	dvModuleInfo info;
	dv::ModuleLibrary library;
	// Run status.
	struct RunControl run;
	// Logging.
	dv::LogBlock logger;
	// I/O connectivity.
	std::unordered_map<std::string, ModuleInput> inputs;
	std::unordered_map<std::string, ModuleOutput> outputs;
	// Input data availability.
	struct InputDataAvailable dataAvailable;
	// Module thread management.
	std::thread thread;
	std::atomic_bool threadAlive;

public:
	Module(std::string_view _name, std::string_view _library);
	~Module();

	void start();

	void registerType(const dv::Types::Type type);
	void registerOutput(std::string_view name, std::string_view typeName);
	void registerInput(std::string_view name, std::string_view typeName, bool optional = false);

	dv::Types::TypedObject *outputAllocate(std::string_view outputName);
	void outputCommit(std::string_view outputName);
	const dv::Types::TypedObject *inputGet(std::string_view inputName);
	void inputDismiss(std::string_view inputName, const dv::Types::TypedObject *data);

	dv::Config::Node outputGetInfoNode(std::string_view outputName);
	const dv::Config::Node inputGetInfoNode(std::string_view inputName);
	bool inputIsConnected(std::string_view inputName);

private:
	void LoggingInit();
	void RunningInit();
	void StaticInit();

	Module *getModule(const std::string &moduleName);
	ModuleOutput *getModuleOutput(const std::string &outputName);
	ModuleInput *getModuleInput(const std::string &outputName);

	static void connectToModuleOutput(ModuleOutput *output, OutConnection connection);
	static void disconnectFromModuleOutput(ModuleOutput *output, OutConnection connection);

	void inputConnectivityInitialize();
	void inputConnectivityDestroy();

	void verifyOutputInfoNodes();
	void cleanupOutputInfoNodes();

	void runThread();
	void runStateMachine();
	void shutdownProcedure(bool doModuleExit, bool disableModule);

	static void moduleShutdownListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
	static void moduleLogLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
	static void moduleConfigUpdateListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
};

} // namespace dv

#endif /* MODULE_HPP_ */
