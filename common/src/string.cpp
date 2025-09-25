#include "sg/string.h"

#include <cstring>
#include <cwchar>
#include <stdexcept>
#include <string>

namespace sg::common {

/* wide-string functions return -1 on erro */
#define THROW_ON_ERROR(fn)                                                                         \
    do {                                                                                           \
        auto const err = (fn);                                                                     \
        if (err == static_cast<size_t>(-1))                                                        \
            throw std::runtime_error(strerror(errno));                                             \
    } while (0)

std::wstring to_wstring(const char* input) {
    thread_local auto state = std::mbstate_t();

    // number of wchars needed (not including terminating null)
    auto len = std::mbsrtowcs(nullptr, &input, 0, &state);
    THROW_ON_ERROR(len);

    // Note, std::wstring::data() already contains a null at the end
    auto result = std::wstring(len, '\0');
    THROW_ON_ERROR(std::mbsrtowcs(result.data(), &input, len, &state));

    return result;
}
std::wstring to_wstring(const std::string& input) {
    return to_wstring(input.c_str());
}

} // namespace sg::common