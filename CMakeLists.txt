# Init CMake (require at least version 2.8.12)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)

# General build settings
IF (NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "Possible build types: None Debug Release RelWithDebInfo MinSizeRel")
ENDIF()

IF (NOT CMAKE_INSTALL_PREFIX)
	IF (UNIX AND NOT APPLE)
		SET(CMAKE_INSTALL_PREFIX "/usr" CACHE STRING "CMake default install prefix, set to /usr on Unix/Linux")
	ELSE()
		MESSAGE(FATAL_ERROR "CMAKE_INSTALL_PREFIX is not set")
	ENDIF()
ENDIF()

# Project name and version
PROJECT(dv-runtime C CXX)
SET(PROJECT_VERSION_MAJOR 1)
SET(PROJECT_VERSION_MINOR 9)
SET(PROJECT_VERSION_PATCH 9)
SET(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

# Define installation paths
INCLUDE(GNUInstallDirs)

# Custom CMake base module
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
  ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/caer /usr/${CMAKE_INSTALL_DATAROOTDIR}/caer
  ${CMAKE_INSTALL_PREFIX}/share/caer /usr/share/caer
  /usr/local/${CMAKE_INSTALL_DATAROOTDIR}/caer /usr/local/share/caer)

# Basic setup for DV-runtime (uses libcaer)
INCLUDE(caer-base)
CAER_SETUP(TRUE)

# Required: threads.
SET(DV_INCDIRS "${USER_LOCAL_INCDIRS}")
SET(DV_LIBDIRS "${USER_LOCAL_LIBDIRS}")
SET(DV_LIBS ${SYSTEM_THREAD_LIBS})

# Search for external libraries with pkg-config.
INCLUDE(FindPkgConfig)

# libcaer devices and types support.
PKG_CHECK_MODULES(LIBCAER REQUIRED libcaer>=3.1.9)
SET(DV_PKGCONFIG_REQUIRES_PRIVATE "libcaer >= 3.1.9")

SET(DV_INCDIRS ${DV_INCDIRS} ${LIBCAER_INCLUDE_DIRS})
SET(DV_LIBDIRS ${DV_LIBDIRS} ${LIBCAER_LIBRARY_DIRS})
SET(DV_LIBS ${DV_LIBS} ${LIBCAER_LIBRARIES})

# Boost support for C++
FIND_PACKAGE(Boost 1.50 REQUIRED COMPONENTS system filesystem iostreams program_options)

SET(DV_INCDIRS ${DV_INCDIRS} ${Boost_INCLUDE_DIRS})
SET(DV_LIBDIRS ${DV_LIBDIRS} ${Boost_LIBRARY_DIRS})
SET(DV_LIBS ${DV_LIBS} ${Boost_LIBRARIES})

IF ((NOT (Boost_MAJOR_VERSION EQUAL 1 AND Boost_MINOR_VERSION GREATER_EQUAL 61))
	OR (OS_UNIX OR OS_LINUX OR OS_MACOSX))
	# libdl needed if Boost too old to have Boost.DLL as a replacement,
	# or for Boost.DLL itself on some platforms like Linux and MacOS X.
	SET(DV_LIBS ${DV_LIBS} dl)
ENDIF()

# Windows boost.ASIO needs extra libraries
IF (OS_WINDOWS)
	SET(DV_LIBS ${DV_LIBS} wsock32 ws2_32)
ENDIF()

# boost.ASIO SSL needs OpenSSL too
# Use -DOPENSSL_ROOT_DIR=x to select a specific installation.
FIND_PACKAGE(OpenSSL REQUIRED)

SET(DV_INCDIRS ${DV_INCDIRS} ${OPENSSL_INCLUDE_DIR})
SET(DV_LIBS ${DV_LIBS} ${OPENSSL_LIBRARIES})

# OpenCV support.
FIND_PACKAGE(OpenCV 3.1.0 REQUIRED)

SET(DV_INCDIRS ${DV_INCDIRS} ${OpenCV_INCLUDE_DIRS})
SET(DV_LIBS ${DV_LIBS} ${OpenCV_LIBS})

# Add local directory to include and library paths
SET(DV_INCDIRS ${DV_INCDIRS} ${CMAKE_SOURCE_DIR}/)
SET(DV_LIBDIRS ${DV_LIBDIRS} ${CMAKE_SOURCE_DIR}/)

# Set include/library paths for successive compilation.
INCLUDE_DIRECTORIES(${DV_INCDIRS})
LINK_DIRECTORIES(${DV_LIBDIRS})

MESSAGE(STATUS "Include directories: ${DV_INCDIRS}")
MESSAGE(STATUS "Link directories: ${DV_LIBDIRS}")

# Define module installation paths.
SET(DV_MODULES_DIR ${CMAKE_INSTALL_DATAROOTDIR}/dv/modules)
MESSAGE(STATUS "Final modules installation directory is: ${CMAKE_INSTALL_PREFIX}/${DV_MODULES_DIR}")
ADD_DEFINITIONS(-DDV_MODULES_DIR=${USER_LOCAL_PREFIX}/${DV_MODULES_DIR})

# Install SDK library.
ADD_SUBDIRECTORY(dv-sdk)

# Compile libdvsdk and dv-runtime main executable.
ADD_SUBDIRECTORY(src)

# Compile extra modules and utilities.
ADD_SUBDIRECTORY(modules)
ADD_SUBDIRECTORY(utils)

# Generate pkg-config file
CONFIGURE_FILE(libdvsdk.pc.in libdvsdk.pc @ONLY)

INSTALL(FILES ${CMAKE_BINARY_DIR}/libdvsdk.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# Install CMake file for modules
INSTALL(FILES dv-modules.cmake DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/dv)
