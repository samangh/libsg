#include "sg/tcp_native.h"

#include <limits>
#include <stdexcept>

#if defined(_WIN32)
    #include <Mswsock.h>
    #include <Winsock2.h>
    #include <Ws2tcpip.h>
#else
    #include <netinet/in.h>
    #include <netinet/ip.h>
    #include <netinet/tcp.h>
    #include <sys/socket.h>
#endif

namespace sg::net::native {

void set_keepalive(socket_t nativeHandle, bool enableKeepAlive, unsigned idleSec,
                   unsigned intervalSec, unsigned count) {

    /* note:
     *  - in boost could have used teh set_option(..) to enable keep-alive, but that does not
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
    setsockopt(nativeHandle, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepAliveInt,
               sizeof(keepAliveInt));

    if (enableKeepAlive) {
#if defined(__APPLE__)
        // Apple only allow setting idle time, cast others to void to prevent compiler warning about
        // unused variables
        (void)intervalSec;
        (void)count;
        setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPALIVE, &idleSec, sizeof(idleSec));
#else
        setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPIDLE, (const char*)&idleSec, sizeof(idleSec));
        setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPINTVL, (const char*)&intervalSec,
                   sizeof(intervalSec));
        setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPCNT, (const char*)&count, sizeof(count));
#endif
    }
}

void set_timeout(socket_t nativeHandle, unsigned timeoutMSec) {
#if defined(_WIN32)
    setsockopt(nativeHandle, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMSec,
               sizeof(timeoutMSec));
    setsockopt(nativeHandle, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutMSec,
               sizeof(timeoutMSec));
#else
    const timeval tv{.tv_sec = timeoutMSec / 1000, .tv_usec = (timeoutMSec % 1000) * 1000};
    setsockopt(nativeHandle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(nativeHandle, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}
} // namespace sg::net::native