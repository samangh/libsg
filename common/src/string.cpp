#include "sg/string.h"

#include <cstring>
#include <cwchar>
#include <locale>
#include <stdexcept>
#include <string>

/* wide-string functions return -1 on erro */
#define THROW_ON_ERROR(fn)                                                                         \
    do {                                                                                           \
        auto const err = (fn);                                                                     \
        if (err == static_cast<size_t>(-1))                                                        \
            throw std::runtime_error(strerror(errno));                                             \
    } while (0)

#define TRY_EXIT_ON_SUCCESS(fn)                                                                    \
    do {                                                                                           \
        try {                                                                                      \
            (fn);                                                                                  \
            return;                                                                                \
        } catch (...) {}                                                                           \
    } while (0)

namespace {
bool localname_has_utf8(const std::string& input) {
    return input.ends_with("UTF8") || input.ends_with("UTF-8") || input.ends_with("utf8") ||
           input.ends_with("utf-8") || input.ends_with("Utf8") || input.ends_with("Utf-8");
}

void set_global_locale_ctype(const std::string& ctype) {
    /* Note:
     *
     * The C locale and the C++ locales are mostly unrelated. However, making a C++ locale
     * object global via locale::global() affects the global C locale and results in a call to
     * setlocale(). When this happens, locale-sensitive C functions called from within a C++ program
     * will use the global C++ locale.
     *
     * There is no way to affect the C++ locale from within a C program.
     *
     * See https://www.chem.kuleuven.be/peeters/manuals/pgi/doc/pgC++_lib/stdlibug/sta_9169.htm
     */

    // create a copy of the default locale, the copy the CTYPE part from the user environment
    std::locale newLocale(std::locale(), ctype, std::locale::ctype);
    std::locale::global(newLocale);

    // On macOS, setting C++ ctype locale seems to not propagate LC_CTYPE
    std::setlocale(LC_CTYPE, ctype.c_str());
}
} // namespace

namespace sg::common {

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

std::wstring to_wstring(const std::string& input) { return to_wstring(input.c_str()); }

void set_global_utf8_codepage() {
    // If default LC_CTYPE uses UTF8 codepage, then exit
    if (localname_has_utf8(std::setlocale(LC_CTYPE, nullptr)))
        return;

    // If the user-environment uses UTF8 codepage, create a copy of the default locale, then
    // copy the CTYPE part from the user-environment
    if (localname_has_utf8(std::locale("").name()))
        TRY_EXIT_ON_SUCCESS(set_global_locale_ctype(""));

    // Try different possibilities one by one until one works
#if defined(_WIN32)
    // Use current locale, just change to UTF8 codepage (Windows only)
    TRY_EXIT_ON_SUCCESS(set_global_locale_ctype(".utf8"));
#endif
    TRY_EXIT_ON_SUCCESS(set_global_locale_ctype("C.utf8"));
    TRY_EXIT_ON_SUCCESS(set_global_locale_ctype("en_US.utf8"));
    TRY_EXIT_ON_SUCCESS(set_global_locale_ctype("en_GB.utf8"));
}

} // namespace sg::common
