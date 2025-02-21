cmake_minimum_required(VERSION 3.15)

project(libsg
  VERSION "1.0.0"
  DESCRIPTION "libsg"
  LANGUAGES C CXX)

set(NAMESPACE SG)

##
## CMake module paths
##
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/external/libsg_cmake/cmake")

##
## Pre-amble
##
include(DefaultPreamble)

##
## Additional Options
##
option(BENCHMARK "Run benchmarks" OFF)
option(LIBSG_IMGUI "Include IMGUI stuff" ON)
option(LIBSG_OWN_LIBUV "Use our own copy of libuv" OFF)

##
## Packages
##

find_package(Threads REQUIRED)
find_package(fmt REQUIRED)

##
## Targets/Sources
##

add_subdirectory(common)

if(LIBSG_IMGUI)
  add_subdirectory(imgui)
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(test)
endif()

##
## Postamble
##
include(DefaultPostamble)
