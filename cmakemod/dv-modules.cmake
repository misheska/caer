FUNCTION(DV_MODULE_SETUP)

SET(CMAKE_C_STANDARD 11 PARENT_SCOPE)
SET(CMAKE_C_STANDARD_REQUIRED ON PARENT_SCOPE)
SET(CMAKE_CXX_STANDARD 17 PARENT_SCOPE)
SET(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)

# Call caer-base CMake module to setup all variables
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
	${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/caer /usr/${CMAKE_INSTALL_DATAROOTDIR}/caer
	${CMAKE_INSTALL_PREFIX}/share/caer /usr/share/caer
	/usr/local/${CMAKE_INSTALL_DATAROOTDIR}/caer /usr/local/share/caer)

# Basic setup for DV modules (uses libcaer)
INCLUDE(caer-base)
CAER_SETUP()

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
	SET(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} PARENT_SCOPE)
	SET(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} PARENT_SCOPE)
ENDIF()

# libcaer devices and types support.
FIND_PACKAGE(libcaer REQUIRED)
IF (libcaer_VERSION VERSION_LESS "3.1.9")
	MESSAGE(FATAL_ERROR "Cannot find libcaer 3.1.9 or newer.")
ENDIF()

SET(DV_LIBRARIES libcaer::caer)

# DV SDK support.
FIND_PACKAGE(dv REQUIRED)
IF (dv_VERSION VERSION_LESS "1.9.9")
	MESSAGE(FATAL_ERROR "Cannot find libdvsdk 1.9.9 or newer.")
ENDIF()

SET(DV_LIBRARIES ${DV_LIBRARIES} dv::dvsdk)

# Boost support for C++
FIND_PACKAGE(Boost 1.50 REQUIRED COMPONENTS system filesystem)

SET(DV_LIBRARIES ${DV_LIBRARIES} Boost::boost Boost::system Boost::filesystem)

# OpenCV support.
FIND_PACKAGE(OpenCV REQUIRED COMPONENTS core imgproc)
IF (OpenCV_VERSION VERSION_LESS "3.1.0")
	MESSAGE(FATAL_ERROR "Cannot find OpenCV 3.1.0 or newer.")
ENDIF()

SET(DV_LIBRARIES ${DV_LIBRARIES} ${OpenCV_LIBS})

# Add local source directory to include path
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/)

MESSAGE(STATUS "Base libraries: ${DV_LIBRARIES}")

# Define module installation paths.
SET(DV_MODULES_DIR ${CMAKE_INSTALL_DATAROOTDIR}/dv/modules)
MESSAGE(STATUS "Final modules installation directory is: ${CMAKE_INSTALL_PREFIX}/${DV_MODULES_DIR}")
ADD_DEFINITIONS(-DDV_MODULES_DIR=${USER_LOCAL_PREFIX}/${DV_MODULES_DIR})

# Set build variables in parent scope
SET(DV_LIBRARIES ${DV_LIBRARIES} PARENT_SCOPE)
SET(DV_MODULES_DIR ${DV_MODULES_DIR} PARENT_SCOPE)

ENDFUNCTION()
