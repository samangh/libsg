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

void SG_COMMON_EXPORT set_reuse_address(socket_t nativeHandle, bool enbaled);

/** Sets the SO_EXCLUSIVEADDRUSE option in Windows. Is a no-op in other systems.
 *
 * See https://learn.microsoft.com/en-us/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse */
void SG_COMMON_EXPORT set_exclusive_addr_use(socket_t, bool enabled);

void SG_COMMON_EXPORT set_recv_buffer_size(socket_t nativeHandle, int size);
void SG_COMMON_EXPORT set_send_buffer_size(socket_t nativeHandle, int size);
int SG_COMMON_EXPORT get_recv_buffer_size(socket_t nativeHandle);
int SG_COMMON_EXPORT get_send_buffer_size(socket_t nativeHandle);

}