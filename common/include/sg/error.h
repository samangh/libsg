#pragma once
#include <sg/export/common.h>

#include <stdexcept>
#include <string>

/* get error message from errno */
#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #include <Windows.h>
    #include <Winsock2.h>

    #define THROW_ON_ERRORNO(fn)                                                                   \
        do {                                                                                       \
            if ((fn) == 0) /* On Windows, most function return 0 on error */                       \
                throw std::runtime_error(sg::error::get_windows_error_message(GetLastError()));    \
        } while (0)

    #define THROW_ON_ERRORNO_SOCKET(fn) do {                                                       \
        if ((fn) == SOCKET_ERROR)                                                                  \
                throw std::runtime_error(sg::error::get_windows_error_message(WSAGetLastError())); \
        } while (0)
#else
    #include <cerrno>
    #include <cstring>

    #define THROW_ON_ERRORNO(fn)                                                                   \
        do {                                                                                       \
            if ((fn) != 0)                                                                         \
                throw std::runtime_error(strerror(errno));                                         \
        } while (0)
   #define THROW_ON_ERRORNO_SOCKET(fn) THROW_ON_ERRORNO(fn)
#endif

namespace sg::error {

#if defined(_WIN32)
std::string SG_COMMON_EXPORT get_windows_error_message(DWORD errorID);
#endif

} // namespace sg::windows

