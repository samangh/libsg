# Set this to OFF to build static libraries
option (BUILD_SHARED_LIBS "Build shared libraries" ON)

if (BUILD_SHARED_LIBS)
  set(USE_STATIC_LIBS_DEFAULT OFF)
else()
  set(USE_STATIC_LIBS_DEFAULT ON)
endif()
option (USE_STATIC_LIBS "Use external static libs if possible" ${USE_STATIC_LIBS_DEFAULT})

if(MSVC)
  option (USE_STATIC_RUNTIME "Statically link against the C++ runtime" USE_STATIC_LIBS)
endif()

option (IPO "Enable inter-process and link-time optimisation" OFF)
option (ARCH_NATIVE "Optimise code for current architecture" OFF)
option (USE_SSE "Enable global use of SSE if possible" ARCH_NATIVE)

# By default, install headers. Note that you can always choose the component wih `--component` flag
option (INSTALL_${PROJECT_NAME}_HEADERS "Install project headers as part of cmake install" ${PROJECT_IS_TOP_LEVEL})
option (INSTALL_${PROJECT_NAME}_BINARIES "Install project binaries as part of cmake install" ${PROJECT_IS_TOP_LEVEL})

option (SANITIZE "Enable address, eak and undefined Behaviour sanitizers" OFF)
# Note, the sanitizer also provides SANITIZE_THREAD and SANITIZE_MEMORY options

##
## Static linking (Windows)
##

# Allows for setting MSVC static runtime
if(USE_STATIC_RUNTIME)
  # Enable policy for subprojects that are using an old cmake_minimum_version
  set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(USE_STATIC_LIBS)
  include(prioritise_static_libraries)
  prioritise_static_libraries()
endif()

##
## Enable link-time optimisation for all targets
##

if(IPO)
  # Enable policy for subprojects that are using an old cmake_minimum_version
  set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

  include(CheckIPOSupported)
  check_ipo_supported(RESULT IPO_SUPPORTED)
  if (IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endif()

##
## IDE support
##

# Export compile database for IDEs, needed for QtCreator
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# This enables SSE later for all targets
if(USE_SSE)
  find_package(SSE OPTIONAL_COMPONENTS SSE42 AVX2)
endif()

##
## CMake module paths
##

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../external/sanitizers-cmake/cmake")

##
## Global properties
##

# Disable additional targets from being created when using CTest
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)

##
## Import functions
##

include(setup_ide_folders)
include(get_standard_library_name)
include(SetupTarget)

# Check for name of standard library, use by common
include(CheckCXXSourceCompiles)
get_standard_library_name(STANDARD_LIBRARY)

# CPM
include(get_cpm)

##
## Install path
##
include(GNUInstallDirs)

# Place libraries in same location as executables in Windows
if(WIN32 OR MSYS2)
  set(CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_BINDIR})
endif()

##
## Packages
##

# Setup boost variables, in case any of our projects use it
include(setup_boost)
find_package(Sanitizers REQUIRED)

# Enable default SANitizer options
if(SANITIZE)
  set(SANITIZE_ADDRESS ON)
  set(SANITIZE_UNDEFINED ON)
endif()

##
## Global project version
##

string(TOLOWER ${NAMESPACE} NAMESPACE_LOWER)

configure_file (
  "${CMAKE_CURRENT_LIST_DIR}/version.h.in"
  "${CMAKE_BINARY_DIR}/include/${NAMESPACE_LOWER}/export/${PROJECT_NAME}_version.h"
)

install(
  FILES "${CMAKE_BINARY_DIR}/include/${NAMESPACE_LOWER}/export/${PROJECT_NAME}_version.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${NAMESPACE_LOWER}/export/"
  COMPONENT dev
)
