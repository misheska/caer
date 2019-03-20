# SDK support library.
SET(LIBDVSDK_SRC_FILES
	module_sdk.cpp
	mainloop_sdk.cpp
	portability_sdk.cpp
	types.cpp
	config_tree/tree.cpp
	config_tree/helper.cpp
	config_tree/node.cpp)

# Set full RPATH
SET(CMAKE_INSTALL_RPATH ${USER_LOCAL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

# Compile SDK library.
ADD_LIBRARY(dvsdk SHARED ${LIBDVSDK_SRC_FILES})

SET_TARGET_PROPERTIES(dvsdk
    PROPERTIES
    SOVERSION ${PROJECT_VERSION_MAJOR}
    VERSION ${PROJECT_VERSION}
)

TARGET_LINK_LIBRARIES(dvsdk ${DV_LIBS})

IF (OS_WINDOWS)
    INSTALL(TARGETS dvsdk RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
ELSE()
    INSTALL(TARGETS dvsdk DESTINATION ${CMAKE_INSTALL_LIBDIR})
ENDIF()

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
	mainloop.cpp
	service.cpp
	main.cpp)

# Set full RPATH
SET(CMAKE_INSTALL_RPATH ${USER_LOCAL_PREFIX}/${CMAKE_INSTALL_BINDIR})

# Compile main dv-runtime executable.
ADD_EXECUTABLE(dv-runtime ${DV_SRC_FILES})
TARGET_LINK_LIBRARIES(dv-runtime ${DV_LIBS} dvsdk)
INSTALL(TARGETS dv-runtime DESTINATION ${CMAKE_INSTALL_BINDIR})

# TCMalloc only makes sense for the main executable
IF (NOT USE_TCMALLOC)
	SET(USE_TCMALLOC 0 CACHE BOOL "Link to and use TCMalloc (Google Perftools) to provide faster memory allocation.")
ENDIF()

# TCMalloc support (fast memory allocation)
IF (USE_TCMALLOC)
	PKG_CHECK_MODULES(LIBTCMALLOC REQUIRED libtcmalloc>=2.2)

	TARGET_INCLUDE_DIRECTORIES(dv-runtime PRIVATE ${LIBTCMALLOC_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(dv-runtime ${LIBTCMALLOC_LIBRARIES})
ENDIF()
