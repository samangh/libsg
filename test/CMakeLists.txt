set(SG_TARGET_NANE "test")
set(TARGET "sg_${SG_TARGET_NANE}")

set_property(GLOBAL APPEND PROPERTY ${PROJECT_NAME}_TARGETS ${TARGET})

##
## External projects
##

CPMAddPackage("gh:doctest/doctest@2.4.11")
CPMAddPackage(NAME nanobench
  GITHUB_REPOSITORY martinus/nanobench
  VERSION 4.3.11
  GIT_SHALLOW)

##
## Source files
##

file(GLOB_RECURSE SRC_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(${TARGET} ${SRC_FILES})
add_executable(${NAMESPACE}::${SG_TARGET_NANE} ALIAS ${TARGET})

##
## Includes
##

target_include_directories(${TARGET}
  INTERFACE
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  PRIVATE)

target_link_libraries(${TARGET}
  PRIVATE
    SG::common
    nanobench
    doctest::doctest)

##
## Additional flags
##

target_compile_options(${TARGET} PRIVATE
  $<$<COMPILE_LANG_AND_ID:CXX,GNU>:--coverage>)

target_link_options(${TARGET} PRIVATE
  $<$<COMPILE_LANG_AND_ID:CXX,GNU>:--coverage>)

if(BENCHMARK)
  target_compile_definitions(${TARGET} PRIVATE BENCHMARK)
endif()

##
## Copy dependencies
##

# On DLL paltforms (e.g. Windows), copy dependent libraries to executable folder. Has no effect on other systems
# Note: requires Cmake 3.26 or higher
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.26.0")
  add_custom_command(TARGET ${TARGET} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${TARGET}> $<TARGET_RUNTIME_DLLS:${TARGET}>
    COMMAND_EXPAND_LISTS)
endif()

##
## Enable testing
##

enable_testing()
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(${TARGET})
