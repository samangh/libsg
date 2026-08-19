#include "sg/net/tcp_native.h"
#include "sg/net/net.h"

#include "sg/error.h"

#include <limits>
#include <stdexcept>
#if defined(_WIN32)
    // careful, order of these headers can matter
    #include <Winsock2.h>
    #include <Mswsock.h>
    #include <Ws2tcpip.h>
#else
    #include <netinet/in.h>
    #include <netinet/ip.h>
    #include <netinet/tcp.h>
    #include <sys/socket.h>
#endif

/* Apple calls the idle time TCP_KEEPALIVE; everywhere else it is TCP_KEEPIDLE */
#ifndef TCP_KEEPIDLE
    #define TCP_KEEPIDLE TCP_KEEPALIVE
#endif

namespace {

/* Linux stores twice what setsockopt(SO_RCVBUF/SO_SNDBUF) is given -- the extra half is kernel
 * bookkeeping rather than payload -- and getsockopt reports that doubled figure. Halving it makes
 * the getter report the application-visible size, which is both the value the setter was handed
 * and the figure Boost reports for the same socket: boost/asio/detail/impl/socket_ops.ipp does the
 * identical /2, and tcp_session::reader() already sizes its read buffer from that view.
 *
 * macOS and Windows do not double. */
int application_visible_buffer_size(int raw) {
#if defined(__linux__)
    return raw / 2;
#else
    return raw;
#endif
}

} // namespace

namespace sg::net::native {

void set_keepalive(socket_t nativeHandle, keepalive_t keepAlive) {
    auto enableKeepAlive = keepAlive.enable;
    auto idleSec = keepAlive.idle_seconds;
    auto intervalSec = keepAlive.interval_seconds;
    auto count = keepAlive.count;

    /* note:
     *  - in boost could have used the set_option(..) to enable keep-alive, but that does not
     *    modify the interval values, etc
     *  - values have to be passed as `const char *` in Windows, but `void *` in Linux/Apple. As
     *    `void *` can automatically take `const char *`, let's use `const char *` everywhere
     *  - Windows actually can take `uint32_t` here, but let's just limit to INTMAX for sake of
     *    consistency
     */

    // You can cast `unsinged *` to `int *` safely as long as it's less than INTMAX, as otherwise
    // the int* becomes -ve.
    //
    // The (std::numeric_limits<int>::max)() syntax prevents confusion with the `max` macro defined
    // in windows headers
    if (idleSec > (std::numeric_limits<int>::max)() ||
        intervalSec > (std::numeric_limits<int>::max)() ||
        count > (std::numeric_limits<int>::max)())
        throw std::invalid_argument("keepalive parameters are out of range");

    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPIDLE, (const char*)&idleSec, sizeof(idleSec)));
    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPINTVL, (const char*)&intervalSec, sizeof(intervalSec)));
    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPCNT, (const char*)&count, sizeof(count)));

    int keepAliveInt = enableKeepAlive;
    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepAliveInt, sizeof(keepAliveInt)));
}

void set_timeout(socket_t nativeHandle, unsigned timeoutMSec) {
#if defined(_WIN32)
    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMSec,
               sizeof(timeoutMSec)));
    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutMSec,
               sizeof(timeoutMSec)));
#else
    timeval tv;
    tv.tv_sec  = timeoutMSec / 1000;
    tv.tv_usec = (timeoutMSec % 1000) * 1000;

    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)));
    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)));
#endif
}
void set_reuse_address(socket_t nativeHandle, bool enabled) {
    int enableInt = enabled;
    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, SOL_SOCKET, SO_REUSEADDR,
                                       (const char*)&enableInt, sizeof(enableInt)));
}

void set_exclusive_addr_use([[maybe_unused]] socket_t nativeHandle, [[maybe_unused]] bool enabled) {
#ifdef _WIN32
    int enableInt = enabled;
    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                                       (const char*)&enableInt, sizeof(enableInt)));
#endif
}

void set_recv_buffer_size(socket_t nativeHandle, int size) {
    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, SOL_SOCKET, SO_RCVBUF,
                                       (const char*)&size, sizeof(size)));
}

void set_send_buffer_size(socket_t nativeHandle, int size) {
    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, SOL_SOCKET, SO_SNDBUF,
                                       (const char*)&size, sizeof(size)));
}

int get_recv_buffer_size(socket_t nativeHandle) {
    int size = 0;
#if defined(_WIN32)
    int len = sizeof(size);
#else
    socklen_t len = sizeof(size);
#endif
    THROW_ON_ERRORNO_SOCKET(getsockopt(nativeHandle, SOL_SOCKET, SO_RCVBUF,
                                       (char*)&size, &len));

    return application_visible_buffer_size(size);
}

int get_send_buffer_size(socket_t nativeHandle) {
    int size = 0;
#if defined(_WIN32)
    int len = sizeof(size);
#else
    socklen_t len = sizeof(size);
#endif
    THROW_ON_ERRORNO_SOCKET(getsockopt(nativeHandle, SOL_SOCKET, SO_SNDBUF, (char*)&size, &len));

    return application_visible_buffer_size(size);
}

} // namespace sg::net::native
