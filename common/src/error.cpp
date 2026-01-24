#include "sg/error.h"

namespace sg::error {

#if defined(_WIN32)
std::string get_windows_error_message(DWORD errorID) {
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
#endif

} // namespace sg::windows

