if(LIBSG_ZSTD)
  find_package(zstd REQUIRED)
endif()

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
  LINK_INTERFACE
  LINK_PUBLIC
    libuv::libuv
    fmt::fmt
    Boost::boost
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
    src/debug.cpp
    src/file_writer.cpp
    src/gettimeofday.cpp
    src/net.cpp
    src/notifiable_background_worker.cpp
    src/progress.cpp
    src/version.cpp
    src/tcp_listener.cpp
    src/libuv_wrapper.cpp
    src/crc.cpp
    src/tcp_session.cpp
    src/tcp_server.cpp
    src/tcp_context.cpp
    src/tcp_client.cpp
    src/uuid.cpp
)

if(LIBSG_ZSTD)
  target_sources(sg_common
    PRIVATE
      src/compression_zstd.cpp)
endif()
