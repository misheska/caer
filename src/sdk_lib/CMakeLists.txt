# SDK support library.
SET(LIBDVSDK_SRC_FILES
	# Forward to runtime library part.
	sdk_base.cpp
	sdk_module.cpp
	# Standalone library part.
	sdk_cross.cpp
	../config_tree/tree.cpp
	../config_tree/helper.cpp
	../config_tree/node.cpp)

# Set full RPATH
SET(CMAKE_INSTALL_RPATH ${USER_LOCAL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

# Compile SDK library.
ADD_LIBRARY(dvsdk SHARED ${LIBDVSDK_SRC_FILES})

SET_TARGET_PROPERTIES(dvsdk
	PROPERTIES
	SOVERSION ${PROJECT_VERSION_MAJOR}
	VERSION ${PROJECT_VERSION}
)

TARGET_LINK_LIBRARIES(dvsdk
	PUBLIC
		libcaer::caer
		Boost::boost
	PRIVATE
		Boost::system
		Boost::filesystem
		Boost::iostreams)

IF (OS_WINDOWS)
	INSTALL(TARGETS dvsdk EXPORT dv-exports RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
ELSE()
	INSTALL(TARGETS dvsdk EXPORT dv-exports DESTINATION ${CMAKE_INSTALL_LIBDIR})
ENDIF()
