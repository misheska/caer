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
#include <deque>
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
	std::string linkedOutput;
	libcaer::ringbuffer::RingBuffer<IntrusiveTypedObject *> queue;
	std::vector<boost::intrusive_ptr<IntrusiveTypedObject>> inUsePackets;

	ModuleInput(const dv::Types::Type &t, bool opt) :
		type(t),
		optional(opt),
		linkedOutput(nullptr),
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

class OutConnection {
public:
	libcaer::ringbuffer::RingBuffer<IntrusiveTypedObject *> *queue;
	InputDataAvailable *dataAvailable;

	OutConnection(libcaer::ringbuffer::RingBuffer<IntrusiveTypedObject *> *queue_, InputDataAvailable *dataAvailable_) :
		queue(queue_),
		dataAvailable(dataAvailable_) {
	}

	bool operator==(const OutConnection &rhs) const noexcept {
		return ((queue == rhs.queue) && (dataAvailable == rhs.dataAvailable));
	}

	bool operator!=(const OutConnection &rhs) const noexcept {
		return (!operator==(rhs));
	}
};

class ModuleOutput {
public:
	dv::Types::Type type;
	dv::Config::Node infoNode;
	std::mutex destinationsLock;
	std::vector<OutConnection> destinations;
	boost::intrusive_ptr<IntrusiveTypedObject> nextPacket;

	ModuleOutput(const dv::Types::Type &t, dv::Config::Node info) : type(t), infoNode(info) {
	}
};

enum class ModuleCommand {
	START    = 0,
	SHUTDOWN = 1,
	RESTART  = 2,
	TS_RESET = 3,
};

struct ModuleControl {
	ModuleCommand cmd;
	uint64_t id;
};

class Module : public dvModuleDataS {
private:
	// Module info.
	std::string name;
	dvModuleInfo info;
	dv::ModuleLibrary library;
	// Run status.
	std::mutex runLock;
	std::condition_variable runCond;
	bool running;
	bool isRunning;
	std::atomic_bool configUpdate;
	// Command and control.
	std::mutex controlLock;
	std::deque<dv::ModuleControl> controlQueue;
	static std::atomic_uint64_t controlIDGenerator;
	std::mutex controlDestinationsLock;
	std::vector<std::deque<dv::ModuleControl> *> controlDestinations;
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

	static std::pair<std::string, std::string> parseModuleInputString(const std::string &moduleInput);

	Module *getModule(const std::string &moduleName);
	ModuleOutput *getModuleOutput(const std::string &outputName);
	ModuleInput *getModuleInput(const std::string &outputName);

	static void connectToModuleOutput(ModuleOutput *output, OutConnection connection);
	static void disconnectFromModuleOutput(ModuleOutput *output, OutConnection connection);

	bool inputConnectivityInitialize();
	void inputConnectivityDestroy();

	bool verifyOutputInfoNodes();
	void cleanupOutputInfoNodes();

	void runThread();
	void runStateMachine();
	void handleModuleInitFailure(bool doModuleExit);
	void shutdownProcedure(bool doModuleExit);

	static void moduleShutdownListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
	static void moduleLogLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
	static void moduleConfigUpdateListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
};

} // namespace dv

#endif /* MODULE_HPP_ */
