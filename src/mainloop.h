#ifndef MAINLOOP_H_
#define MAINLOOP_H_

#include "dv-sdk/mainloop.h"
#include "dv-sdk/module.h"

#include "module.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dv {

class MainData {
public:
	dv::Config::Node configNode;
	std::atomic_bool systemRunning;
	std::unordered_map<std::string, dv::Module> modules;
	dv::Types::TypeSystem typeSystem;

	MainData() : configNode(nullptr), systemRunning(false) {
	}
};

/*
 * Run application (data processing).
 */
void MainRun(void);

/**
 * Only for internal usage! Do not reset the main data pointer!
 */
void MainSDKLibInit(MainData *setMainDataPtr);

} // namespace dv

#endif /* MAINLOOP_H_ */
