#pragma once
#include <sg/export/common.h>

#if defined(_WIN32)
    #include <Winsock2.h>
#endif

namespace sg::net::native {

#if defined(_WIN32)
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

/** Enables/disables TCP keepalive. If the keep-alive set to be disabled then the additional
 * parameters are ignored.
 *
 * @param nativeHandle Native platform socket handle
 * @param enableKeepAlive Set true to enable keepalive, false to disable
 * @param idleSec Number of seconds of no-activity before a keepalive packet is sent
 * @param intervalSec Interal between each keep alive packet
 * @param count Number of keepalive packets to send before terminating the connection
 */
void SG_COMMON_EXPORT set_keepalive(socket_t nativeHandle, bool enableKeepAlive, unsigned idleSec,
                                    unsigned intervalSec, unsigned count);

void SG_COMMON_EXPORT set_timeout(socket_t nativeHandle, unsigned timeoutMSec);
}