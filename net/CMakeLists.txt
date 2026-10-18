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
    $<$<BOOL:${LIBSG_TLS}>:
      src/tls.cpp
      src/tcp_transport_tls.cpp>
    $<IF:$<BOOL:${WIN32}>,
      src/interfaces_win32.cpp,
      src/interfaces_posix.cpp>
    src/tcp_transport_plain.cpp
    src/tcp_transport.cpp
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
    $<$<BOOL:${LIBSG_TLS}>:OpenSSL::SSL OpenSSL::Crypto> # Public because sg/net/tls.h exposes boost::asio::ssl::context
    $<$<OR:$<BOOL:${MSYS2}>,$<BOOL:${WIN32}>>:Mswsock Ws2_32> # Needed for Boost ASIO, must be public becaue we use ASIO in public headers
  LINK_PRIVATE
    $<$<BOOL:${WIN32}>:iphlpapi> # for GetAdaptersAddresses() in src/interfaces_win32.cpp
  COMPILE_OPTIONS_INTERFACE
  COMPILE_OPTIONS_PUBLIC
  COMPILE_OPTIONS_PRIVATE
  COMPILE_FEATURES_INTERFACE
  COMPILE_FEATURES_PUBLIC
    cxx_std_20
  COMPILE_FEATURES_PRIVATE
  COMPILE_DEFINITIONS_PUBLIC
    $<$<BOOL:${LIBSG_TLS}>:LIBSG_NET_TLS>
    # FMT_HEADER_ONLY

  ## Options
  GENERATE_EXPORT_HEADER
  VISIBILITY_HIDDEN
  # STATIC
)
