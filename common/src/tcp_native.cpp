#include "sg/tcp_native.h"
#include "sg/tcp_session.h"

#if defined(_WIN32)
#include <Winsock2.h>
#include <Mswsock.h>
#endif

namespace sg::net::native {

void set_keepalive(socket_t nativeHandle, bool enableKeepAlive, uint32_t idleSec, uint32_t intervalSec,
                   uint32_t count) {

    /* note:
     *
     *  - in boost could have used teh set_option(..) to enable keep-alive, but that does not
     *    modify the interval values, etc
     *  - values have to be passed as `const char *` in Windows
     */

    int keepAliveInt = enableKeepAlive;
    #if defined(_WIN32)
    setsockopt(nativeHandle, SOL_SOCKET, SO_KEEPALIVE, (const char*)&keepAliveInt, sizeof(keepAliveInt));
    #else
    setsockopt(nativeHandle, SOL_SOCKET, SO_KEEPALIVE, &keepAliveInt, sizeof(keepAliveInt));
    #endif

    if (enableKeepAlive) {
        #if defined(__APPLE__)
        //Apple only allow setting idle time
        setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPALIVE, &idleSec, sizeof(idleSec));
        #elif defined(_WIN32)
        setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPIDLE, (const char*)&idleSec, sizeof(idleSec));
        setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPINTVL, (const char*)&intervalSec, sizeof(intervalSec));
        setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPCNT, (const char*)&count, sizeof(count));
        #else
        setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPIDLE, &idleSec, sizeof(idleSec));
        setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPINTVL, &intervalSec, sizeof(intervalSec));
        setsockopt(nativeHandle, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
        #endif
    }
}

void set_timeout(socket_t nativeHandle, uint32_t timeoutMSec) {
    #if defined(_WIN32)
    setsockopt(nativeHandle, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMSec, sizeof(timeoutMSec));
    setsockopt(nativeHandle, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutMSec, sizeof(timeoutMSec));
    #else
    const timeval tv{.tv_sec = timeoutMSec / 1000, .tv_usec = (timeoutMSec % 1000) * 1000} ;
    setsockopt(nativeHandle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(nativeHandle, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    #endif
}
}