find_package(Threads REQUIRED)
find_package(LibUV REQUIRED)
find_package(fmt REQUIRED)
find_package(zstd REQUIRED)

# Needed hardware-assited CRC32C checksum, optional as the SSE package
# will set pre-processor header definitions and the code will check for
# that
if (USE_SSE)
  find_package(SSE OPTIONAL_COMPONENTS SSE42 CRC32 CLMUL)
endif ()

# Boost is required for other CRC checksums
find_package(Boost REQUIRED CONFIG)

##
## External files
##

set(DP_THREAD_POOL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/thread-pool)

##
## Source files
##


setup_library(
  TARGET
    sg_common
  NAMESPACE
    SG
  NAMESPACE_TARGET
     common
  DIRECTORY
     ${CMAKE_CURRENT_SOURCE_DIR}

  INCLUDE_INTERFACE
  INCLUDE_PUBLIC
    ${DP_THREAD_POOL_DIR}/include
  INCLUDE_PRIVATE
    Boost::Boost
  LINK_INTERFACE
  LINK_PUBLIC
    LibUV::LibUV
    fmt::fmt
  LINK_PRIVATE
    $<$<BOOL:${SSE_SSE42_FOUND}>:SSE::SSE42> # For hardware accelerated CRC32
    $<$<BOOL:${SSE_CRC32_FOUND}>:SSE::CRC32> # For hardware accelerated CRC32
    $<$<BOOL:${SSE_CLMUL_FOUND}>:SSE::CLMUL> # For hardware accelerated CRC32
    Threads::Threads
    $<$<BOOL:${WIN32}>:Winmm>
    $<IF:$<BOOL:${USE_STATIC_LIBS}>,zstd::libzstd_static,zstd::libzstd_shared>
  COMPILE_OPTIONS_INTERFACE
  COMPILE_OPTIONS_PUBLIC
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<STREQUAL:${STANDARD_LIBRARY},c++>>:-fexperimental-library> # for std::jthread with libc++
  COMPILE_OPTIONS_PRIVATE
  COMPILE_FEATURES_INTERFACE
  COMPILE_FEATURES_PUBLIC
    cxx_std_20
  COMPILE_FEATURES_PRIVATE

  ## Options
  GENERATE_EXPORT_HEADER
  # STATIC
)

