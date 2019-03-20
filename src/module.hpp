#ifndef MODULE_HPP_
#define MODULE_HPP_

#include <libcaercpp/ringbuffer.hpp>

#include "dv-sdk/module.h"

#include "log.hpp"
#include "mainloop.h"
#include "types.hpp"

#include <string>
#include <utility>

#define INTER_MODULE_TRANSFER_QUEUE_SIZE 256

// If Boost version recent enough, use their portable DLL loading support.
// Else use dlopen() on POSIX systems.
#include <boost/version.hpp>
#if defined(BOOST_VERSION) && (BOOST_VERSION / 100000) == 1 && (BOOST_VERSION / 100 % 1000) >= 61
#	define BOOST_HAS_DLL_LOAD 1
#else
#	define BOOST_HAS_DLL_LOAD 0
#endif

#if BOOST_HAS_DLL_LOAD
#	include <boost/dll.hpp>
#else
#	include <dlfcn.h>
#endif

namespace dv {

#if BOOST_HAS_DLL_LOAD
using ModuleLibrary = boost::dll::shared_library;
#else
using ModuleLibrary = void *;
#endif

void ModulesUpdateInformationListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

// Module-related definitions.
enum class ModuleStatus {
	STOPPED = 0,
	RUNNING = 1,
};

class ModuleInput {
public:
	dv::Types::Type type;
	bool optional;
	libcaer::ringbuffer::RingBuffer queue;

	ModuleInput(const dv::Types::Type &t, bool opt) : type(t), optional(opt), queue(INTER_MODULE_TRANSFER_QUEUE_SIZE) {
	}
};

class ModuleOutput {
public:
	dv::Types::Type type;

	ModuleOutput(const dv::Types::Type &t) : type(t) {
	}
};

class Module {
private:
	std::string name;
	dvModuleInfo info;
	ModuleLibrary library;
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

	void handleModuleInitFailure();

	static void moduleShutdownListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
	static void moduleLogLevelListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
	static void moduleConfigUpdateListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);

	static void inputListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
		const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
};

} // namespace dv

#endif /* MODULE_HPP_ */
