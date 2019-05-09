# Init CMake (require at least version 3.12.0)
CMAKE_MINIMUM_REQUIRED(VERSION 3.12.0)

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
PROJECT(dv-runtime
	VERSION 1.9.9
	LANGUAGES C CXX)
SET(CMAKE_C_STANDARD 11)
SET(CMAKE_C_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define installation paths
INCLUDE(GNUInstallDirs)

# Custom CMake base module
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmakemod
	${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/caer /usr/${CMAKE_INSTALL_DATAROOTDIR}/caer
	${CMAKE_INSTALL_PREFIX}/share/caer /usr/share/caer
	/usr/local/${CMAKE_INSTALL_DATAROOTDIR}/caer /usr/local/share/caer)

# Basic setup for DV-runtime (uses libcaer)
INCLUDE(caer-base)
CAER_SETUP()

# Setup atomics.
INCLUDE(CheckAtomic)

# libcaer devices and types support.
FIND_PACKAGE(libcaer REQUIRED)
IF (libcaer_VERSION VERSION_LESS "3.1.9")
	MESSAGE(FATAL_ERROR "Cannot find libcaer 3.1.9 or newer.")
ENDIF()
SET(DV_PKGCONFIG_REQUIRES_PRIVATE "libcaer >= 3.1.9")

# Boost support for C++
FIND_PACKAGE(Boost 1.50 REQUIRED COMPONENTS system filesystem iostreams program_options)

# boost.ASIO needs threading
SET(BOOST_ASIO_LIBRARIES ${SYSTEM_THREAD_LIBS})

# Windows boost.ASIO needs extra libraries
IF (OS_WINDOWS)
	SET(BOOST_ASIO_LIBRARIES ${BOOST_ASIO_LIBRARIES} wsock32 ws2_32)
ENDIF()

# boost.ASIO SSL needs OpenSSL too
# Use -DOPENSSL_ROOT_DIR=x to select a specific installation.
FIND_PACKAGE(OpenSSL REQUIRED)

SET(BOOST_ASIO_LIBRARIES ${BOOST_ASIO_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto)

# OpenCV support.
FIND_PACKAGE(OpenCV REQUIRED COMPONENTS core imgproc)
IF (OpenCV_VERSION VERSION_LESS "3.1.0")
	MESSAGE(FATAL_ERROR "Cannot find OpenCV 3.1.0 or newer.")
ENDIF()

# We always refer back to dv-sdk/ for example.
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/)

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

# Export the cmake configuration for the install tree
INCLUDE(CMakePackageConfigHelpers)
WRITE_BASIC_PACKAGE_VERSION_FILE(
	${PROJECT_BINARY_DIR}/dvConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion)
SET(include_dirs ${CMAKE_INSTALL_INCLUDEDIR})
SET(export_destination ${CMAKE_INSTALL_LIBDIR}/cmake/dv)
CONFIGURE_PACKAGE_CONFIG_FILE(
	${PROJECT_SOURCE_DIR}/cmakemod/dvConfig.cmake.in
	${PROJECT_BINARY_DIR}/dvConfig.cmake
	INSTALL_DESTINATION ${export_destination}
	PATH_VARS include_dirs export_destination)
INSTALL(EXPORT dv-exports
	NAMESPACE dv::
	DESTINATION ${export_destination})
INSTALL(FILES
	${PROJECT_BINARY_DIR}/dvConfigVersion.cmake
	${PROJECT_BINARY_DIR}/dvConfig.cmake
	DESTINATION ${export_destination})
UNSET(include_dirs)
UNSET(export_destination)

# Install CMake file for modules
INSTALL(FILES cmakemod/dv-modules.cmake DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/dv)
