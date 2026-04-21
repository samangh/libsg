#include "sg/tcp_native.h"

#include "sg/error.h"
#include "sg/net.h"

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

    int keepAliveInt = enableKeepAlive;
    THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepAliveInt,
               sizeof(keepAliveInt)));

    if (enableKeepAlive) {
#if defined(__APPLE__)
        // Apple only allow setting idle time, cast others to void to prevent compiler warning about
        // unused variables
        (void)intervalSec;
        (void)count;
        THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPALIVE, &idleSec, sizeof(idleSec)));
#else
        THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPIDLE, (const char*)&idleSec, sizeof(idleSec)));
        THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPINTVL, (const char*)&intervalSec, sizeof(intervalSec)));
        THROW_ON_ERRORNO_SOCKET(setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPCNT, (const char*)&count, sizeof(count)));
#endif
    }
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
void set_reuse_address(socket_t nativeHandle, bool enbaled) {
    int enableInt = enbaled;
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

    return size;
}

int get_send_buffer_size(socket_t nativeHandle) {
    int size = 0;
#if defined(_WIN32)
    int len = sizeof(size);
#else
    socklen_t len = sizeof(size);
#endif
    THROW_ON_ERRORNO_SOCKET(getsockopt(nativeHandle, SOL_SOCKET, SO_SNDBUF, (char*)&size, &len));

    return size;
}

} // namespace sg::net::native
