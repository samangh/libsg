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
option (BENCHMARK "Run benchmarks" OFF)

##
## Packages
##

find_package(Threads REQUIRED)
find_package(fmt REQUIRED)

##
## Targets/Sources
##

add_subdirectory(common)
add_subdirectory(imgui)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(test)
endif()

##
## Postamble
##
include(DefaultPostamble)
