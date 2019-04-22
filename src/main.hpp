#ifndef MAIN_HPP_
#define MAIN_HPP_

#include "types.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace dv {

class Module;

struct SDKLibFunctionPointers {
	// Type interface.
	std::function<const dv::Types::Type(const char *, const dv::Module *)> getTypeInfoCharString;
	std::function<const dv::Types::Type(uint32_t, const dv::Module *)> getTypeInfoIntegerID;
	// Module interface.
	std::function<void(dv::Module *, const dv::Types::Type)> registerType;
	std::function<void(dv::Module *, std::string_view, std::string_view)> registerOutput;
	std::function<void(dv::Module *, std::string_view, std::string_view, bool)> registerInput;
	std::function<dv::Types::TypedObject *(dv::Module *, std::string_view)> outputAllocate;
	std::function<void(dv::Module *, std::string_view)> outputCommit;
	std::function<const dv::Types::TypedObject *(dv::Module *, std::string_view)> inputGet;
	std::function<void(dv::Module *, std::string_view, const dv::Types::TypedObject *)> inputDismiss;
	std::function<dv::Config::Node(dv::Module *, std::string_view)> outputGetInfoNode;
	std::function<const dv::Config::Node(dv::Module *, std::string_view)> inputGetInfoNode;
	std::function<bool(dv::Module *, std::string_view)> inputIsConnected;
};

class MainData {
public:
	std::atomic_bool systemRunning;
	std::mutex modulesLock;
	std::unordered_map<std::string, std::shared_ptr<dv::Module>> modules;
	dv::Types::TypeSystem typeSystem;
	dv::SDKLibFunctionPointers libFunctionPointers;

	static MainData &getGlobal() {
		static MainData md;
		return (md);
	}

private:
	MainData() : systemRunning(true) {
	}
};

/**
 * Adds a new module to the system, initializing it and
 * its static configuration.
 *
 * @param name new module instance name.
 * @param library shared library plugin to load.
 */
void addModule(const std::string &name, const std::string &library);

/**
 * Removes a module from the system, fully erasing its configuration.
 * You must ensure the module was stopped first!
 *
 * @param name module instance name.
 */
void removeModule(const std::string &name);

/**
 * Only for internal usage! Do not reset the main data pointer!
 */
void SDKLibInit(const SDKLibFunctionPointers *setLibFuncPtr);

extern const SDKLibFunctionPointers *glLibFuncPtr;

} // namespace dv

#endif /* MAIN_HPP_ */
