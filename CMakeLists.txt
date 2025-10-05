cmake_minimum_required(VERSION 3.15)

project(libsg
  VERSION "1.0.0"
  DESCRIPTION "libsg"
  LANGUAGES CXX)

set(NAMESPACE SG)

##
## Additional Options
##
option(LIBSG_BUILD_TESTING "Build test suite" ${BUILD_TESTING})
option(LIBSG_BUILD_DOCS "Generate documentation" OFF)
option(LIBSG_IMGUI "Include IMGUI stuff" ON)
option(LIBSG_ZSTD "Include zstd stuff" ON)

if(LIBSG_IMGUI)
  if(WIN32)
    option(LIBSG_IMGUI_DIRECTX "Enable DirectX ImGUI backend" ON)
    option(LIBSG_IMGUI_OPENGL "Enable OpenGL ImGUI backend" OFF)
  else()
    # No choice really, have to use OpenGL in other systems
    option(LIBSG_IMGUI_OPENGL "Enable OpenGL ImGUI backend" ON)
  endif()

  if(NOT LIBSG_IMGUI_DIRECTX AND NOT LIBSG_IMGUI_OPENGL)
    message(FATAL_ERROR "Need either OpenGL or DirectX for ImGUI")
  endif()
endif()

set(BUILD_DOCS ${LIBSG_BUILD_DOCS})

##
## CMake module paths
##
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/external/libsg_cmake/cmake")

##
## Pre-amble
##
include(DefaultPreamble)


##
## Packages
##

# Boost is required
find_package(Boost REQUIRED CONFIG)

# Some platforms don't have a Findfmt file, so search if the user has used ONE_LIBFMT first
if(NOT OWN_FMT)
  find_package(fmt REQUIRED)
endif()

if(NOT OWN_UV)
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
