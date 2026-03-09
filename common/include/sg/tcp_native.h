#pragma once
#include <sg/export/common.h>
#include "net.h"

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
 * parameters are ignored */
void SG_COMMON_EXPORT set_keepalive(socket_t nativeHandle, keepalive_t keepAlive);

void SG_COMMON_EXPORT set_timeout(socket_t nativeHandle, unsigned timeoutMSec);
}