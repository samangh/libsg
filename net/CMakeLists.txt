##
## Source files
##

setup_library(
  TARGET
    sg_net
  NAMESPACE
    ${NAMESPACE}
  NAMESPACE_TARGET
     net
  DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}

  SOURCES_PRIVATE
    src/net.cpp
    src/net_uv.cpp
    src/tcp_session.cpp
    src/tcp_server.cpp
    src/asio_io_pool.cpp
    src/tcp_client.cpp
    src/tcp_client_sync.cpp
    src/tcp_native.cpp

  INCLUDE_INTERFACE
  INCLUDE_PUBLIC
  INCLUDE_PRIVATE
  LINK_INTERFACE
  LINK_PUBLIC
    sg::common
    Boost::boost
    $<$<OR:$<BOOL:${MSYS2}>,$<BOOL:${WIN32}>>:Mswsock Ws2_32> # Needed for Boost ASIO, must be public becaue we use ASIO in public headers
  LINK_PRIVATE
    libuv::libuv
  COMPILE_OPTIONS_INTERFACE
  COMPILE_OPTIONS_PUBLIC
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
  VISIBILITY_HIDDEN
  # STATIC
)

set_property(TARGET sg_common PROPERTY OUTPUT_NAME sg)

# For legacy users
add_library(SG::common ALIAS sg_common)
