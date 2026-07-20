cmake_minimum_required(VERSION 3.15)

project(libsg
  VERSION "1.0.0"
  DESCRIPTION "libsg"
  LANGUAGES CXX)

set(NAMESPACE sg)

##
## Additional Options
##
option(LIBSG_IMGUI "Include IMGUI stuff" ON)
option(LIBSG_ZSTD "Include zstd stuff" ON)
option(LIBSG_STACKTRACE "Enable boost::stacktrace support" OFF)
option(LIBSG_EXCEPTION_DETAILS "Show function name, etc. when exception is thrown with SG_THROW" OFF)

# If not top-level project, disable test and documentation by default
if(NOT PROJECT_IS_TOP_LEVEL)
  option(LIBSG_BUILD_TESTING "Build test suite" OFF)
  option(LIBSG_BUILD_DOCS "Generate documentation" OFF)
  set(BUILD_DOCS ${LIBSG_BUILD_DOCS})
  set(BUILD_TESTING ${LIBSG_BUILD_TESTING})
endif()

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

##
## CMake module paths
##
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

##
## Pre-amble
##
include(DefaultPreamble)

##
## Packages
##

# Boost is required
find_package(Boost 1.86 REQUIRED CONFIG)

# Boost::stacktrace
if(LIBSG_STACKTRACE)
  # Needed to print traces properly when using boost::stacktrace in non-MSVC systems
  set(CMAKE_EXECUTABLE_ENABLE_EXPORTS ON)

  if(MSVC OR MINGW)
    set(SG_BOOST_STACKTRACE_COMP stacktrace_windbg)
  else()
    set(SG_BOOST_STACKTRACE_COMP stacktrace_basic)
  endif()
  find_package(Boost COMPONENTS ${SG_BOOST_STACKTRACE_COMP} REQUIRED CONFIG)

  if(SANITIZE_THREAD)
    message(WARNING "Both SANITIZE_THREAD and LIBSG_STACKTRACE are ON. This is not a good idea if you are throwing exceptions in quick succession, as LIBSG_STACKTRACE will fork addr2line in quick sucession and cause TSAN to fail.")
  endif()
endif()

# Some platforms don't have a Findfmt file, so search if the user has used OWN_FMT first
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
add_subdirectory(net)

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
