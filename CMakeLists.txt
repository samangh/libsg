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
option(LIBSG_IMGUI "Include IMGUI stuff" ON)
option(LIBSG_BUILD_TESTING "Build test suite" ${BUILD_TESTING})

##
## Packages
##

find_package(Threads REQUIRED)

# Some platforms don't have a Findfmt file, so search if the user has used ONE_LIBFMT first
if(NOT OWN_LIBFMT)
  find_package(fmt REQUIRED)
endif()

if(NOT OWN_LIBUV)
  find_package(libuv REQUIRED)
endif()

##
## Targets/Sources
##

add_subdirectory(common)

if(LIBSG_IMGUI)
  add_subdirectory(imgui)
endif()

include(CTest)
if(LIBSG_BUILD_TESTING)
  add_subdirectory(test)
endif()

##
## Postamble
##
include(DefaultPostamble)
