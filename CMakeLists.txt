cmake_minimum_required(VERSION 3.15.0)

project(libsg
  VERSION "1.0.0"
  DESCRIPTION "libsg"
  LANGUAGES C CXX)

set(NAMESPACE SG)

##
## CMake module paths
##

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

##
## Build options
##

include(DefaultPreamble)
option (BENCHMARK "Run benchmarks" OFF)

##
## Packages
##

# Setup boost variables, in case any of our projects use it
find_package(Threads REQUIRED)
find_package(fmt REQUIRED)

##
## Targets/Sources
##

add_subdirectory(common)
add_subdirectory(imgui)

if(BUILD_TESTS)
  include(CTest)
  add_subdirectory(test)
endif()

##
## Compile options
##

# Generate target list
get_property(TARGETS GLOBAL PROPERTY ${PROJECT_NAME}_TARGETS)

include(setup_target_options)
foreach(t ${TARGETS})
  setup_target_options(TARGET ${t})
  target_compile_features(${t} PUBLIC cxx_std_20)
endforeach()

include(DefaultPostamble)
