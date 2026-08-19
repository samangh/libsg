#pragma once
#include <sg/export/net.h>
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

/** sets TCP socket keepalive parameters */
void SG_NET_EXPORT set_keepalive(socket_t nativeHandle, keepalive_t keepAlive);

/* sets timeout for BLOCKING read/write operations. Does not apply to async operations */
void SG_NET_EXPORT set_timeout(socket_t nativeHandle, unsigned timeoutMSec);

void SG_NET_EXPORT set_reuse_address(socket_t nativeHandle, bool enabled);

/** Sets the SO_EXCLUSIVEADDRUSE option in Windows. Is a no-op in other systems.
 *
 * See https://learn.microsoft.com/en-us/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse */
void SG_NET_EXPORT set_exclusive_addr_use(socket_t, bool enabled);

/** @brief Sets the socket receive/send buffer size.
 *
 * The kernel may clamp the request to its own minimum, or to a system-wide maximum
 * (@c net.core.rmem_max / @c wmem_max on Linux), so the size granted can differ from the one
 * asked for. Read it back with the matching getter to find out what was granted. */
void SG_NET_EXPORT set_recv_buffer_size(socket_t nativeHandle, int size);
void SG_NET_EXPORT set_send_buffer_size(socket_t nativeHandle, int size);

/** @brief The socket receive/send buffer size available to the application.
 *
 * On Linux the raw @c SO_RCVBUF / @c SO_SNDBUF value is twice the value that you set, the
 * surplus being kernel bookkeeping rather than payload. Our function these report the halved figure,
 * which is the actual data that the application will see. */
int SG_NET_EXPORT get_recv_buffer_size(socket_t nativeHandle);
int SG_NET_EXPORT get_send_buffer_size(socket_t nativeHandle);

}