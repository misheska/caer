#ifndef MODULE_H_
#define MODULE_H_

#include "dv-sdk/module.h"

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
using ModuleLibrary = boost::dll::shared_library;
#else
#	include <dlfcn.h>
using ModuleLibrary = void *;
#endif

#include <string>
#include <utility>

std::pair<ModuleLibrary, dvModuleInfo> dvModuleLoadLibrary(const std::string &moduleName);
void dvModuleUnloadLibrary(ModuleLibrary &moduleLibrary);
void dvUpdateModulesInformation();

#endif /* MODULE_H_ */
