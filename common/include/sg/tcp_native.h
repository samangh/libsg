#pragma once
#include <cstdint>
#include <sg/export/common.h>

namespace sg::net::native {

#if defined(_WIN32)
typedef int SOCKET;
#else
typedef int socket_t;
#endif

void SG_COMMON_EXPORT set_keepalive(socket_t nativeHandle, bool enableKeepAlive,
                                    std::uint32_t idleSec, uint32_t intervalSec, uint32_t count);

void SG_COMMON_EXPORT set_timeout(socket_t nativeHandle, uint32_t timeoutMSec);
}