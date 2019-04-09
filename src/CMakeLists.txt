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
	main.cpp)

# Set full RPATH
SET(CMAKE_INSTALL_RPATH ${USER_LOCAL_PREFIX}/${CMAKE_INSTALL_BINDIR})

# Compile main dv-runtime executable.
ADD_EXECUTABLE(dv-runtime ${DV_SRC_FILES})
TARGET_LINK_LIBRARIES(dv-runtime ${DV_LIBS} dvsdk)
INSTALL(TARGETS dv-runtime DESTINATION ${CMAKE_INSTALL_BINDIR})

# Atomics support for older architectures/systems.
IF (NEED_LIBATOMIC)
	MESSAGE(STATUS "Using libatomic for std::atomic.")

	TARGET_INCLUDE_DIRECTORIES(dv-runtime PRIVATE atomic)
	TARGET_LINK_LIBRARIES(dv-runtime PRIVATE atomic)
ENDIF()

# TCMalloc only makes sense for the main executable
IF (NOT ENABLE_TCMALLOC)
	SET(ENABLE_TCMALLOC 0 CACHE BOOL "Link to and use TCMalloc (Google Perftools) to provide faster memory allocation.")
ENDIF()

# TCMalloc support (fast memory allocation)
IF (ENABLE_TCMALLOC)
	PKG_CHECK_MODULES(LIBTCMALLOC REQUIRED libtcmalloc>=2.2)

	TARGET_INCLUDE_DIRECTORIES(dv-runtime PRIVATE ${LIBTCMALLOC_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(dv-runtime PRIVATE ${LIBTCMALLOC_LIBRARIES})
ENDIF()
