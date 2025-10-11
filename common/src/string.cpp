#include "sg/string.h"

#include <cstring>
#include <cwchar>
#include <locale>
#include <stdexcept>
#include <string>

/* wide-string functions return -1 on error */
#define THROW_ON_ERROR(fn)                                                                         \
    do {                                                                                           \
        auto const err = (fn);                                                                     \
        if (err == static_cast<size_t>(-1))                                                        \
            throw std::runtime_error(strerror(errno));                                             \
    } while (0)

namespace sg::string {

std::wstring to_wstring(const char* input) {
    std::mbstate_t state{};

    // number of wchars needed (not including terminating null)
    auto len = std::mbsrtowcs(nullptr, &input, 0, &state);
    THROW_ON_ERROR(len);

    // Note, std::wstring::data() already contains a null at the end
    auto result = std::wstring(len, '\0');
    THROW_ON_ERROR(std::mbsrtowcs(result.data(), &input, len, &state));

    return result;
}

std::string to_string(const wchar_t* input) {
    std::mbstate_t state{};

    // number of chars needed (not including terminating null)
    auto len = std::wcsrtombs(nullptr, &input, 0, &state);
    THROW_ON_ERROR(len);

    // Note, std::string::data() already contains a null at the end
    auto result = std::string(len, '\0');
    THROW_ON_ERROR(std::wcsrtombs(result.data(), &input, len, &state));

    return result;
}

std::wstring to_wstring(const std::string& input) { return to_wstring(input.c_str()); }
std::string to_string(const std::wstring& input) { return to_string(input.c_str()); }

std::string to_string(const sg::IBuffer<std::byte>& input) {
    return {(const char*)(&*input.begin()), (const char*)(&*input.end())};
}

std::string toupper(std::string s) {
    std::ranges::transform(s, s.begin(), [](const unsigned char c) { return std::toupper(c); });
    return s;
}

std::string tolower(std::string s) {
    std::ranges::transform(s, s.begin(), [](const unsigned char c) { return std::tolower(c); });
    return s;
}

} // namespace sg::common
