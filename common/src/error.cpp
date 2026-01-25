#include "sg/error.h"

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #include <Windows.h>
    #include <Winsock2.h>
#endif

namespace sg::error {

#if defined(_WIN32)
std::string get_windows_error_message(native_error_t errorID) {
    char* s = NULL;
    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, errorID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s, 0,
                      NULL) != 0) {
        auto result = std::string(s);
        LocalFree(s);
        return result;
    } else
        throw std::runtime_error("could not format error message");
}

native_error_t SG_COMMON_EXPORT get_windows_last_error() {
    return GetLastError();
}

native_error_t SG_COMMON_EXPORT get_windows_last_socket_error() {
    return WSAGetLastError();
}

#endif

} // namespace sg::windows

