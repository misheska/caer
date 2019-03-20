#ifndef MODULES_DISCOVERY_HPP_
#define MODULES_DISCOVERY_HPP_

#include "dv-sdk/module.h"

#include <string_view>
#include <utility>

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

std::pair<dv::ModuleLibrary, dvModuleInfo> ModulesLoadLibrary(std::string_view moduleName);
void ModulesUnloadLibrary(dv::ModuleLibrary &moduleLibrary);

void ModulesUpdateInformationListener(dvConfigNode node, void *userData, enum dvConfigAttributeEvents event,
	const char *changeKey, enum dvConfigAttributeType changeType, union dvConfigAttributeValue changeValue);
void ModulesUpdateInformation();

} // namespace dv

#endif /* MODULES_DISCOVERY_HPP_ */
