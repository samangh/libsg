if(LIBSG_ZSTD)
  find_package(zstd REQUIRED)
endif()

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
    Boost::Asio
  INCLUDE_PRIVATE
    Boost::Boost
  LINK_INTERFACE
  LINK_PUBLIC
    libuv::libuv
    fmt::fmt
    $<$<OR:$<BOOL:${MSYS2}>,$<BOOL:${WIN32}>>:Mswsock> # Needed for some Boost ASIO functions, can move to LINK_PRIVATE when tcp_server implementation is moved outside headers
  LINK_PRIVATE
    Threads::Threads
    $<$<BOOL:${WIN32}>:Winmm>
    $<$<BOOL:${LIBSG_ZSTD}>:$<IF:$<BOOL:${USE_STATIC_LIBS}>,zstd::libzstd_static,zstd::libzstd_shared>>
  COMPILE_OPTIONS_INTERFACE
  COMPILE_OPTIONS_PUBLIC
    $<$<AND:$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>,$<STREQUAL:${STANDARD_LIBRARY},c++>>:-fexperimental-library> # for std::jthread with libc++
  COMPILE_OPTIONS_PRIVATE
      $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-strict-aliasing> # recommended by LibUV, otherwise libuv is not safe with optimisations
  COMPILE_FEATURES_INTERFACE
  COMPILE_FEATURES_PUBLIC
    cxx_std_20
  COMPILE_FEATURES_PRIVATE

  COMPILE_DEFINITIONS_PUBLIC
    # FMT_HEADER_ONLY

  ## Options
  GENERATE_EXPORT_HEADER
  DONT_RECURSE_SRC_DIR
  # STATIC
)

target_sources(sg_common
  PRIVATE
    src/accurate_sleeper.cpp
    src/background_timer.cpp
    src/bytes.cpp
    src/cpu.cpp
    src/data_attribute.cpp
    src/data_source.cpp
    src/data_view.cpp
    src/debug.cpp
    src/file_writer.cpp
    src/file_writer_uv.cpp
    src/gettimeofday.cpp
    src/math.cpp
    src/net.cpp
    src/notifiable_background_worker.cpp
    src/progress.cpp
    src/version.cpp
    src/tcp_listener.cpp
    src/libuv_wrapper.cpp
    src/crc.cpp
    src/tcp_session.cpp
    src/tcp_server.cpp
)

if(LIBSG_ZSTD)
  target_sources(sg_common
    PRIVATE
      src/compression_zstd.cpp)
endif()
