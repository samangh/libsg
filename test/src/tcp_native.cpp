#include <catch2/catch_test_macros.hpp>

#include <sg/net/tcp_native.h>

#include <boost/asio.hpp>

#if defined(_WIN32)
    // careful, order of these headers can matter
    #include <Winsock2.h>
    #include <Mswsock.h>
    #include <Ws2tcpip.h>
#else
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/socket.h>
#endif

using namespace sg::net;

namespace {

/* mirrors the option pick in tcp_native.cpp: Apple calls the idle time TCP_KEEPALIVE */
#ifndef TCP_KEEPIDLE
    #define TCP_KEEPIDLE TCP_KEEPALIVE
#endif


int get_opt(native::socket_t handle, int level, int optname) {
    int value = 0;
#if defined(_WIN32)
    int len = sizeof(value);
#else
    socklen_t len = sizeof(value);
#endif
    REQUIRE(::getsockopt(handle, level, optname, (char*)&value, &len) == 0);
    return value;
}

} // namespace

/* A socket's keepalive timings must always be whatever was last asked for, so that the values the
 * session reports through options_t are the values the kernel actually holds.
 *
 * These previously only applied when keepalive was being *enabled*: disabling left the old timings
 * in place, and on Apple the interval and count were dropped on the floor entirely. */
TEST_CASE("sg::net::native: set_keepalive() applies idle, interval and count", "[sg::net::native]") {
    boost::asio::io_context context;
    boost::asio::ip::tcp::socket socket(context);
    socket.open(boost::asio::ip::tcp::v4());
    const auto handle = socket.native_handle();

    native::set_keepalive(handle,
                          {.enable = true, .idle_seconds = 111, .interval_seconds = 7, .count = 3});

    REQUIRE(get_opt(handle, SOL_SOCKET, SO_KEEPALIVE) != 0);
    REQUIRE(get_opt(handle, IPPROTO_TCP, TCP_KEEPIDLE) == 111);
    REQUIRE(get_opt(handle, IPPROTO_TCP, TCP_KEEPINTVL) == 7);
    REQUIRE(get_opt(handle, IPPROTO_TCP, TCP_KEEPCNT) == 3);
}

TEST_CASE("sg::net::native: set_keepalive() applies the timings when disabling too",
          "[sg::net::native]") {
    boost::asio::io_context context;
    boost::asio::ip::tcp::socket socket(context);
    socket.open(boost::asio::ip::tcp::v4());
    const auto handle = socket.native_handle();

    native::set_keepalive(handle,
                          {.enable = true, .idle_seconds = 111, .interval_seconds = 7, .count = 3});

    /* disable, asking for different timings: the socket must take the new ones rather than keep
     * the ones it was last enabled with */
    native::set_keepalive(handle,
                          {.enable = false, .idle_seconds = 222, .interval_seconds = 9, .count = 5});

    REQUIRE(get_opt(handle, SOL_SOCKET, SO_KEEPALIVE) == 0);
    REQUIRE(get_opt(handle, IPPROTO_TCP, TCP_KEEPIDLE) == 222);
    REQUIRE(get_opt(handle, IPPROTO_TCP, TCP_KEEPINTVL) == 9);
    REQUIRE(get_opt(handle, IPPROTO_TCP, TCP_KEEPCNT) == 5);
}

/* re-enabling must not go through a window where the previous call's timings are live, so the
 * timings are written before the enable bit */
TEST_CASE("sg::net::native: re-enabling keepalive uses the timings from the same call",
          "[sg::net::native]") {
    boost::asio::io_context context;
    boost::asio::ip::tcp::socket socket(context);
    socket.open(boost::asio::ip::tcp::v4());
    const auto handle = socket.native_handle();

    native::set_keepalive(handle,
                          {.enable = true, .idle_seconds = 111, .interval_seconds = 7, .count = 3});
    native::set_keepalive(handle,
                          {.enable = false, .idle_seconds = 222, .interval_seconds = 9, .count = 5});
    native::set_keepalive(handle,
                          {.enable = true, .idle_seconds = 333, .interval_seconds = 11, .count = 2});

    REQUIRE(get_opt(handle, SOL_SOCKET, SO_KEEPALIVE) != 0);
    REQUIRE(get_opt(handle, IPPROTO_TCP, TCP_KEEPIDLE) == 333);
    REQUIRE(get_opt(handle, IPPROTO_TCP, TCP_KEEPINTVL) == 11);
    REQUIRE(get_opt(handle, IPPROTO_TCP, TCP_KEEPCNT) == 2);
}
