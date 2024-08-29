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

setup_executable(
  TARGET
    sg_test
  NAMESPACE
    SG
  NAMESPACE_TARGET
     test

  INCLUDE_PUBLIC
    ${DP_THREAD_POOL_DIR}/include
  INCLUDE_PRIVATE
    Boost::Boost

  LINK_PRIVATE
    SG::common
    nanobench
    doctest::doctest

  COMPILE_FEATURES_PUBLIC
    cxx_std_20
  COMPILE_OPTIONS_PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU>:--coverage>
  LINK_OPTIONS_PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU>:--coverage>

  COMPILE_DEFINITIONS_PRIVATE
    $<$<BOOL:BENCHMARK>:BENCHMARK>
)

##
## Enable testing
##

enable_testing()
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(sg_test)
