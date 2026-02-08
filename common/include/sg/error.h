#pragma once
#include <sg/export/common.h>
#include <sg/debug.h>

#include <stdexcept>
#include <string>
#include <cstdint>

#if defined(_WIN32)
    /* I could move this into the implementation file, and wrap GetLastError/WSAGetLastError() in
     * there, but there is no point. An application making use of these macros will have almost
     * certainly already included Windows.h */

    #pragma push_macro("WIN32_LEAN_AND_MEAN")
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>  //for GetLastError() / DWORD
    #include <Winsock2.h> //for WSAGetLastError()
    #pragma pop_macro("WIN32_LEAN_AND_MEAN")
#endif

/* get error message from errno */
#if defined(_WIN32)
    #define THROW_ON_ERRORNO(fn)                                                                   \
        do {                                                                                       \
            if ((fn) == 0) /* On Windows, most function return 0 on error */                       \
                SG_THROW(std::runtime_error, sg::error::windows_error_message(GetLastError()));    \
        } while (0)

    #define THROW_ON_ERRORNO_SOCKET(fn)                                                            \
        do {                                                                                       \
            if ((fn) == SOCKET_ERROR)                                                              \
                SG_THROW(std::runtime_error, sg::error::windows_error_message(WSAGetLastError())); \
        } while (0)
#else
    #include <cerrno>
    #include <cstring>

    #define THROW_ON_ERRORNO(fn)                                                                   \
        do {                                                                                       \
            if ((fn) != 0)                                                                         \
                SG_THROW(std::runtime_error, strerror(errno));                                     \
        } while (0)
   #define THROW_ON_ERRORNO_SOCKET(fn) THROW_ON_ERRORNO(fn)
#endif

namespace sg::error {

#if defined(_WIN32)
std::string SG_COMMON_EXPORT windows_error_message(DWORD errorID);
#endif

} // namespace sg::windows

