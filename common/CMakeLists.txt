if(LIBSG_ZSTD)
  find_package(zstd REQUIRED)
endif()

if (LINUX)
  CPMAddPackage(
      NAME pfs
      URL https://github.com/dtrugman/pfs/archive/refs/tags/v0.12.0.zip
      # GITHUB_REPOSITORY ocornut/imgui
      # VERSION 1.91.9b-docking
      # GIT_SHALLOW
      OPTIONS
      "pfs_BUILD_SAMPLES OFF"
      "pfs_BUILD_TESTS OFF"
  )
endif ()

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

  SOURCES_PRIVATE
    src/accurate_sleeper.cpp
    src/background_timer.cpp
    src/bytes.cpp
    src/cpu.cpp
    src/file_writer.cpp
    src/gettimeofday.cpp
    src/net.cpp
    src/notifiable_background_worker.cpp
    src/progress.cpp
    src/version.cpp
    src/libuv_wrapper.cpp
    src/crc.cpp
    src/tcp_session.cpp
    src/tcp_server.cpp
    src/asio_io_pool.cpp
    src/tcp_client.cpp
    src/tcp_native.cpp
    src/uuid.cpp
    src/file.cpp
    src/process.cpp
    src/string.cpp
    src/locale.cpp
    src/error.cpp
    $<$<BOOL:${LIBSG_ZSTD}>:src/compression_zstd.cpp>

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
    $<$<BOOL:${LINUX}>:pfs>
    $<$<BOOL:${LIBSG_STACKTRACE}>:Boost::${SG_BOOST_STACKTRACE_COMP}>
  LINK_PRIVATE
    Threads::Threads
    $<$<BOOL:${WIN32}>:Winmm>
    $<$<BOOL:${WIN32}>:Kernel32.lib> #For sg::process functions
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
    $<$<BOOL:${LIBSG_STACKTRACE}>:LIBSG_STACKTRACE>
    $<$<BOOL:${LIBSG_EXCEPTION_DETAILS}>:LIBSG_EXCEPTION_DETAILS>

    $<$<AND:$<BOOL:${LIBSG_STACKTRACE}>,$<BOOL:${APPLE}>>:BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED>
    # FMT_HEADER_ONLY

  ## Options
  GENERATE_EXPORT_HEADER
  # STATIC
)
