# SDK support library.
ADD_SUBDIRECTORY(sdk_lib)

# Main dv-runtime executable.
SET(DV_SRC_FILES
	log.cpp
	config.cpp
	config_server/config_server_actions.cpp
	config_server/config_server_connection.cpp
	config_server/config_server.cpp
	devices_discovery.cpp
	modules_discovery.cpp
	module.cpp
	types.cpp
	service.cpp
	main.cpp ../dv-sdk/processing.hpp)

# Set full RPATH
SET(CMAKE_INSTALL_RPATH ${USER_LOCAL_PREFIX}/${CMAKE_INSTALL_BINDIR})

# Compile main dv-runtime executable.
ADD_EXECUTABLE(dv-runtime ${DV_SRC_FILES})

TARGET_LINK_LIBRARIES(dv-runtime
	PRIVATE
		dvsdk
		Boost::boost
		Boost::system
		Boost::filesystem
		Boost::program_options
		${BOOST_ASIO_LIBRARIES})

INSTALL(TARGETS dv-runtime DESTINATION ${CMAKE_INSTALL_BINDIR})

# Needed only by dv-runtime.
IF ((NOT (Boost_MAJOR_VERSION EQUAL 1 AND Boost_MINOR_VERSION GREATER_EQUAL 61))
	OR (OS_UNIX OR OS_LINUX OR OS_MACOSX))
	# libdl needed if Boost too old to have Boost.DLL as a replacement,
	# or for Boost.DLL itself on some platforms like Linux and MacOS X.
	TARGET_LINK_LIBRARIES(dv-runtime PRIVATE dl)
ENDIF()

# Atomics support for older architectures/systems.
IF (NEED_LIBATOMIC)
	MESSAGE(STATUS "Using libatomic for std::atomic in dv-runtime.")

	TARGET_LINK_LIBRARIES(dv-runtime PRIVATE atomic)
ENDIF()

# TCMalloc only makes sense for the main executable
IF (NOT ENABLE_TCMALLOC)
	SET(ENABLE_TCMALLOC 0 CACHE BOOL "Link to and use TCMalloc (Google Perftools) to provide faster memory allocation.")
ENDIF()

# TCMalloc support (fast memory allocation)
IF (ENABLE_TCMALLOC)
	# Search for external libraries with pkg-config.
	INCLUDE(FindPkgConfig)

	PKG_CHECK_MODULES(tcmalloc REQUIRED IMPORTED_TARGET libtcmalloc>=2.2)

	TARGET_LINK_LIBRARIES(dv-runtime PRIVATE PkgConfig::tcmalloc)
ENDIF()
