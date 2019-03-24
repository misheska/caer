#ifndef MAINLOOP_H_
#define MAINLOOP_H_

#include "dv-sdk/mainloop.h"

#include "types.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dv {

class Module;

class MainData {
public:
	std::atomic_bool systemRunning;
	std::mutex modulesLock;
	std::unordered_map<std::string, std::shared_ptr<dv::Module>> modules;
	dv::Types::TypeSystem typeSystem;

	MainData() : systemRunning(false) {
	}
};

/*
 * Run application (data processing).
 */
void MainRun(void);

/**
 * Adds a new module to the system, initializing it and
 * its static configuration.
 *
 * @param name new module instance name.
 * @param library shared library plugin to load.
 */
void addModule(std::string_view name, std::string_view library);
/**
 * Removes a module from the system, fully erasing its configuration.
 * You must ensure the module was stopped first!
 *
 * @param name module instance name.
 */
void removeModule(std::string_view name);

/**
 * Only for internal usage! Do not reset the main data pointer!
 */
void MainSDKLibInit(MainData *setMainDataPtr);

} // namespace dv

#endif /* MAINLOOP_H_ */
