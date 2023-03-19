set(TARGET "common")
set_property(GLOBAL APPEND PROPERTY ${PROJECT_NAME}_TARGETS ${TARGET})

find_package(Threads REQUIRED)
find_package(LibUV REQUIRED)

##
## Source files
##

file(GLOB_RECURSE SRC_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(${TARGET} ${SRC_FILES})

##
## Includes
##

target_include_directories(${TARGET}
  INTERFACE
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  PRIVATE
  )

target_link_libraries(${TARGET}
  INTERFACE
  PUBLIC
    LibUV::LibUV
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.1>>:${STANDARD_LIBRARY}fs>
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:${STANDARD_LIBRARY}fs>
  PRIVATE
    Threads::Threads
    "$<$<BOOL:${WIN32}>:Winmm>")

include (GenerateExportHeader)
generate_export_header(${TARGET}
             EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/include/udaq/export/${TARGET}.h")


##
## Compiler flags
##

target_compile_features(${TARGET} PUBLIC cxx_std_20)

##
## Version
##

configure_file (
  "${PROJECT_SOURCE_DIR}/cmake/version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/version.h"
  )

