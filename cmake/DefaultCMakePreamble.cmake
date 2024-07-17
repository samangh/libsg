# Set this to OFF to build static libraries
option (BUILD_SHARED_LIBS "Build shared libraries" ON)

if (BUILD_SHARED_LIBS)
  set(USE_STATIC_LIBS_DEFAULT OFF)
else()
  set(USE_STATIC_LIBS_DEFAULT ON)
endif()

option (USE_STATIC_LIBS "Use external static libs if possible" ${USE_STATIC_LIBS_DEFAULT})
option (USE_STATIC_RUNTIME "Statically link against the C++ runtime" USE_STATIC_LIBS)
option (ARCH_NATIVE "Optimise code for current architecture" OFF)
option (USE_SSE "Enable global use of SSE if possible" ARCH_NATIVE)

option (SANITIZE "Enable address, eak and undefined Behaviour sanitizers" OFF)
# Note, the sanitizer also provides SANITIZE_THREAD and SANITIZE_MEMORY options

if(USE_STATIC_LIBS)
  include(prioritise_static_libraries)
  prioritise_static_libraries()
endif()

if(SANITIZE)
  set(SANITIZE_ADDRESS ON)
  set(SANITIZE_UNDEFINED ON)
endif()

##
## CMake module paths
##

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../external/sanitizers-cmake/cmake")

##
## Compile support
##

# Export compile database for IDEs, needed for QtCreator
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# This enables SSE later for all targets
if(USE_SSE)
  find_package(SSE OPTIONAL_COMPONENTS SSE42 AVX2)
endif()

##
## Import functions
##

include(setup_ide_folders)
include(get_standard_library_name)

# Check for name of standard library, use by common
include(CheckCXXSourceCompiles)
get_standard_library_name(STANDARD_LIBRARY)

##
## Packages
##

# Setup boost variables, in case any of our projects use it
include(setup_boost)
find_package(Sanitizers REQUIRED)

##
## Policies
##

# Allows for setting MSVC static runtime
cmake_policy(SET CMP0091 NEW)
