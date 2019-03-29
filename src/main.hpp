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

class MainData {
public:
	std::atomic_bool systemRunning;
	std::mutex modulesLock;
	std::unordered_map<std::string, std::shared_ptr<dv::Module>> modules;
	dv::Types::TypeSystem typeSystem;

	static MainData &getGlobal() {
		static MainData md;
		return (md);
	}

private:
	MainData() : systemRunning(false) {
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
void SDKLibInit(MainData *setMainDataPtr);

} // namespace dv

#endif /* MAIN_HPP_ */
