#pragma once
#include <sg/export/common.h>

#include <stdexcept>
#include <string>
#include <cstdint>

/* get error message from errno */
#if defined(_WIN32)
    #define THROW_ON_ERRORNO(fn)                                                                   \
        do {                                                                                       \
            if ((fn) == 0) /* On Windows, most function return 0 on error */                       \
                throw std::runtime_error(                                                          \
                    sg::error::get_windows_error_message(sg::error::get_windows_last_error()));    \
        } while (0)

    #define THROW_ON_ERRORNO_SOCKET(fn)                                                            \
        do {                                                                                       \
            if ((fn) == SOCKET_ERROR)                                                              \
                throw std::runtime_error(sg::error::get_windows_error_message(                     \
                    sg::error::get_windows_last_socket_error()));                                  \
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
typedef uint32_t native_error_t;
std::string SG_COMMON_EXPORT get_windows_error_message(native_error_t errorID);
native_error_t SG_COMMON_EXPORT get_windows_last_error();
native_error_t SG_COMMON_EXPORT get_windows_last_socket_error();
#endif

} // namespace sg::windows

