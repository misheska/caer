FUNCTION(DV_MODULE_SETUP)

# Call caer-base CMake module to setup all variables
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
  ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/caer /usr/${CMAKE_INSTALL_DATAROOTDIR}/caer
  ${CMAKE_INSTALL_PREFIX}/share/caer /usr/share/caer
  /usr/local/${CMAKE_INSTALL_DATAROOTDIR}/caer /usr/local/share/caer)

# Basic setup for DV modules (uses libcaer)
INCLUDE(caer-base)
CAER_SETUP(TRUE)

# Set compiler info
SET(CC_CLANG ${CC_CLANG} PARENT_SCOPE)
SET(CC_GCC ${CC_GCC} PARENT_SCOPE)
SET(CC_ICC ${CC_ICC} PARENT_SCOPE)
SET(CC_MSVC ${CC_MSVC} PARENT_SCOPE)

# Set operating system info
SET(OS_UNIX ${OS_UNIX} PARENT_SCOPE)
SET(OS_LINUX ${OS_LINUX} PARENT_SCOPE)
SET(OS_MACOSX ${OS_MACOSX} PARENT_SCOPE)
SET(OS_WINDOWS ${OS_WINDOWS} PARENT_SCOPE)

# Check threads support (almost nobody implements C11 threads yet! C++11 is fine)
SET(HAVE_PTHREADS ${HAVE_PTHREADS} PARENT_SCOPE)
SET(HAVE_WIN32_THREADS ${HAVE_WIN32_THREADS} PARENT_SCOPE)
SET(SYSTEM_THREAD_LIBS ${SYSTEM_THREAD_LIBS} PARENT_SCOPE)

SET(USER_LOCAL_PREFIX ${USER_LOCAL_PREFIX} PARENT_SCOPE)

# Set C/C++ compiler options
SET(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} PARENT_SCOPE)
SET(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} PARENT_SCOPE)

# RPATH settings
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE PARENT_SCOPE)
IF (OS_MACOSX)
	SET(CMAKE_MACOSX_RPATH TRUE PARENT_SCOPE)
ENDIF()

# Linker settings
IF (OS_UNIX AND NOT OS_MACOSX)
	SET(CMAKE_LINK_WHAT_YOU_USE TRUE PARENT_SCOPE)

	SET(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} PARENT_SCOPE)
	SET(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} PARENT_SCOPE)
ENDIF()

# Required: threads.
SET(DV_INCDIRS "${USER_LOCAL_INCDIRS}")
SET(DV_LIBDIRS "${USER_LOCAL_LIBDIRS}")
SET(DV_LIBS ${SYSTEM_THREAD_LIBS})

# Search for external libraries with pkg-config.
INCLUDE(FindPkgConfig)

PKG_CHECK_MODULES(LIBCAER REQUIRED libcaer>=3.1.1)

SET(DV_INCDIRS ${DV_INCDIRS} ${LIBCAER_INCLUDE_DIRS})
SET(DV_LIBDIRS ${DV_LIBDIRS} ${LIBCAER_LIBRARY_DIRS})
SET(DV_LIBS ${DV_LIBS} ${LIBCAER_LIBRARIES})

PKG_CHECK_MODULES(LIBDVSDK REQUIRED libdvsdk>=1.9.9)

SET(DV_INCDIRS ${DV_INCDIRS} ${LIBDVSDK_INCLUDE_DIRS})
SET(DV_LIBDIRS ${DV_LIBDIRS} ${LIBDVSDK_LIBRARY_DIRS})
SET(DV_LIBS ${DV_LIBS} ${LIBDVSDK_LIBRARIES})

# Add local directory to include and library paths
SET(DV_INCDIRS ${DV_INCDIRS} ${CMAKE_SOURCE_DIR}/)
SET(DV_LIBDIRS ${DV_LIBDIRS} ${CMAKE_SOURCE_DIR}/)

# Define module installation paths.
SET(DV_MODULES_DIR ${CMAKE_INSTALL_DATAROOTDIR}/dv/modules)
MESSAGE(STATUS "Final modules installation directory is: ${CMAKE_INSTALL_PREFIX}/${DV_MODULES_DIR}")
ADD_DEFINITIONS(-DDV_MODULES_DIR=${USER_LOCAL_PREFIX}/${DV_MODULES_DIR})

# Set build variables in parent scope
SET(DV_INCDIRS ${DV_INCDIRS} PARENT_SCOPE)
SET(DV_LIBDIRS ${DV_LIBDIRS} PARENT_SCOPE)
SET(DV_LIBS ${DV_LIBS} PARENT_SCOPE)
SET(DV_MODULES_DIR ${DV_MODULES_DIR} PARENT_SCOPE)

ENDFUNCTION()
