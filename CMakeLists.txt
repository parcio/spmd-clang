#===============================================================================
# Based on example from: https://github.com/banach-space/clang-tutor
#===============================================================================
cmake_minimum_required(VERSION 3.13.4)
project(clang-plugin)

#===============================================================================
# 1. VERIFY CLANG INSTALLATION DIR
#===============================================================================
# Set this to a valid Clang installation dir
set(CT_LLVM_INSTALL_DIR "" CACHE PATH "LLVM installation directory")

# A bit of a sanity checking
set(CT_LLVM_INCLUDE_DIR "${CT_LLVM_INSTALL_DIR}/include/llvm")
if(NOT EXISTS "${CT_LLVM_INCLUDE_DIR}")
message(FATAL_ERROR
  " CT_LLVM_INSTALL_DIR (${CT_LLVM_INCLUDE_DIR}) is invalid.")
endif()

set(CT_LLVM_CMAKE_FILE "${CT_LLVM_INSTALL_DIR}/lib/cmake/clang/ClangConfig.cmake")
if(NOT EXISTS "${CT_LLVM_CMAKE_FILE}")
message(FATAL_ERROR
  " CT_LLVM_CMAKE_FILE (${CT_LLVM_CMAKE_FILE}) is invalid.")
endif()


#===============================================================================
# 2. LOAD CLANG CONFIGURATION
#    For more: http://llvm.org/docs/CMake.html#embedding-llvm-in-your-project
#===============================================================================
list(APPEND CMAKE_PREFIX_PATH "${CT_LLVM_INSTALL_DIR}/lib/cmake/llvm/")
list(APPEND CMAKE_PREFIX_PATH "${CT_LLVM_INSTALL_DIR}/lib/cmake/clang/")

find_package(Clang REQUIRED CONFIG)

# Sanity check. As Clang does not expose e.g. `CLANG_VERSION_MAJOR` through
# AddClang.cmake, we have to use LLVM_VERSION_MAJOR instead.
if(NOT "22" VERSION_EQUAL "${LLVM_VERSION_MAJOR}")
  message(FATAL_ERROR "Found LLVM ${LLVM_VERSION_MAJOR}, but need LLVM 22")
endif()

message(STATUS "Found Clang ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using ClangConfig.cmake in: ${CT_LLVM_INSTALL_DIR}")

message("CLANG STATUS:
  Includes (clang)    ${CLANG_INCLUDE_DIRS}
  Includes (llvm)     ${LLVM_INCLUDE_DIRS}"
)

# Set the LLVM and Clang header and library paths
include_directories(SYSTEM "${LLVM_INCLUDE_DIRS};${CLANG_INCLUDE_DIRS}")


#===============================================================================
# 3. BUILD CONFIGURATION
#===============================================================================
# Use the same C++ standard as LLVM does
set(CMAKE_CXX_STANDARD 20 CACHE STRING "")

# Build type
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE
      STRING "Build type (default Debug):" FORCE)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall\
    -fdiagnostics-color=always")

# LLVM/Clang is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# Set the build directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

#===============================================================================
# 4. ADD TARGETS
#===============================================================================
# add_subdirectory(print-function-names)
add_subdirectory(spmd-plugin)
