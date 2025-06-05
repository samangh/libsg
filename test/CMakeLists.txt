# Note, DON'T use local Catch2, as we wenat catch to be compiled using
# our C++ standard version, otherwise we can't use string vectors, etc
# inside the tests as they would bb passed thorugh a library boundary!
#
# See:
# - https://github.com/catchorg/Catch2/issues/2046
# - https://github.com/catchorg/Catch2/issues/2210

if(WIN32 OR MSYS2)
  set(BUILD_SHARED_LIBS FALSE)
endif()

CPMAddPackage("gh:catchorg/catch2@3.7.0")
target_compile_features(Catch2 PRIVATE cxx_std_20)
target_compile_features(Catch2WithMain PRIVATE cxx_std_20)

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
  DIRECTORY
     ${CMAKE_CURRENT_SOURCE_DIR}

  INCLUDE_PUBLIC
    ${DP_THREAD_POOL_DIR}/include
  INCLUDE_PRIVATE
  LINK_PRIVATE
    SG::common
    Boost::boost
    Catch2::Catch2WithMain

  COMPILE_FEATURES_PUBLIC
    cxx_std_20
  COMPILE_OPTIONS_PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU>:--coverage>
  LINK_OPTIONS_PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU>:--coverage>

  COMPILE_DEFINITIONS_PRIVATE
    $<$<BOOL:${BENCHMARK}>:BENCHMARK>
)

target_sources(sg_test
  PRIVATE
    src/tcp_server.cpp
    src/data/channel_vector.cpp
    src/ranges.cpp
    src/enumeration.cpp
    src/tcp_client.cpp
    src/uuid.cpp
    src/gettimeofday.cpp
)

##
## Enable testing
##

include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(sg_test)
