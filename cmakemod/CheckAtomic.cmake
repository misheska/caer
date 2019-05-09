# ==============================================================================
# LLVM Release License
# ==============================================================================
# University of Illinois/NCSA
# Open Source License
#
# Copyright (c) 2003-2018 University of Illinois at Urbana-Champaign.
# All rights reserved.
#
# Developed by:
#
# LLVM Team
#
# University of Illinois at Urbana-Champaign
#
# http://llvm.org
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal with
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is furnished to do
# so, subject to the following conditions:
#
# * Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimers.
#
# * Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimers in the
# documentation and/or other materials provided with the distribution.
#
# * Neither the names of the LLVM Team, University of Illinois at
# Urbana-Champaign, nor the names of its contributors may be used to
# endorse or promote products derived from this Software without specific
# prior written permission.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE
# SOFTWARE.

# Based on LLVM script with changes to properly detect 32 vs 64 bit atomics support.
INCLUDE(CheckCXXSourceCompiles)
INCLUDE(CheckLibraryExists)

# Sometimes linking against libatomic is required for atomic ops, if
# the platform doesn't support lock-free atomics.

FUNCTION(check_working_cxx_atomics32 varname)
SET(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++11")
CHECK_CXX_SOURCE_COMPILES("
#include <atomic>
#include <cstdint>

std::atomic<uint32_t> x(1);
int main() {
	uint32_t i = x.load(std::memory_order_relaxed);
	x.fetch_add(i);
	return std::atomic_is_lock_free(&x);
}
" ${varname})
SET(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
ENDFUNCTION(check_working_cxx_atomics32)

FUNCTION(check_working_cxx_atomics64 varname)
SET(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
SET(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++11")
CHECK_CXX_SOURCE_COMPILES("
#include <atomic>
#include <cstdint>

std::atomic<uint64_t> x(1);
int main() {
	uint64_t i = x.load(std::memory_order_relaxed);
	x.fetch_add(i);
	return std::atomic_is_lock_free(&x);
}
" ${varname})
SET(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
ENDFUNCTION(check_working_cxx_atomics64)

IF (CC_MSVC)
	# We assume MSVC has it all.
	SET(NEED_LIBATOMIC_FOR32 FALSE)
	SET(NEED_LIBATOMIC_FOR64 FALSE)
ELSE()
	check_working_cxx_atomics32(ATOMIC32_WORK_WITHOUT_LIB)

	IF (ATOMIC32_WORK_WITHOUT_LIB)
		SET(NEED_LIBATOMIC_FOR32 FALSE)
	ELSE()
		CHECK_LIBRARY_EXISTS(atomic __atomic_fetch_add_4 "" HAVE_LIBATOMIC32)

		IF (HAVE_LIBATOMIC32)
			LIST(APPEND CMAKE_REQUIRED_LIBRARIES "atomic")
			check_working_cxx_atomics32(ATOMIC32_WORK_WITH_LIB)

			IF (ATOMIC32_WORK_WITH_LIB)
				SET(NEED_LIBATOMIC_FOR32 TRUE)
			ELSE()
				MESSAGE(FATAL_ERROR "Host compiler doesn't support 32-bit atomics in any configuration.")
			ENDIF()
		ELSE()
			MESSAGE(FATAL_ERROR "Host compiler appears to require libatomic for 32-bit atomics, but cannot find it.")
		ENDIF()
	ENDIF()

	check_working_cxx_atomics64(ATOMIC64_WORK_WITHOUT_LIB)

	IF (ATOMIC64_WORK_WITHOUT_LIB)
		SET(NEED_LIBATOMIC_FOR64 FALSE)
	ELSE()
		CHECK_LIBRARY_EXISTS(atomic __atomic_fetch_add_8 "" HAVE_LIBATOMIC64)

		IF (HAVE_LIBATOMIC64)
			LIST(APPEND CMAKE_REQUIRED_LIBRARIES "atomic")
			check_working_cxx_atomics32(ATOMIC64_WORK_WITH_LIB)

			IF (ATOMIC64_WORK_WITH_LIB)
				SET(NEED_LIBATOMIC_FOR64 TRUE)
			ELSE()
				MESSAGE(FATAL_ERROR "Host compiler doesn't support 64-bit atomics in any configuration.")
			ENDIF()
		ELSE()
			MESSAGE(FATAL_ERROR "Host compiler appears to require libatomic for 64-bit atomics, but cannot find it.")
		ENDIF()
	ENDIF()
ENDIF()

# If any of the require flags are true, also set a generic one.
IF (NEED_LIBATOMIC_FOR32 OR NEED_LIBATOMIC_FOR64)
	SET(NEED_LIBATOMIC TRUE)
ELSE()
	SET(NEED_LIBATOMIC FALSE)
ENDIF()
