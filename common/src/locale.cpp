#include "sg/locale.h"

#include <locale>

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


} // namespace

namespace sg::locale {

void set_ctype_globally(const std::string& ctype) {
    /* Note:
     *
     * The C locale and the C++ locales are mostly unrelated. However, making a C++ locale
     * object global via locale::global() affects the global C locale and results in a call to
     * setlocale(). When this happens, locale-sensitive C functions called from within a C++
     * program will use the global C++ locale.
     *
     * There is no way to affect the C++ locale from within a C program.
     *
     * See https://www.chem.kuleuven.be/peeters/manuals/pgi/doc/pgC++_lib/stdlibug/sta_9169.htm and
     * https://learn.microsoft.com/en-us/cpp/parallel/multithreading-and-locales
     */

    // create a copy of the default locale, the copy the CTYPE part from the user environment
    std::locale newLocale(std::locale(), ctype, std::locale::ctype);
    std::locale::global(newLocale);

    // On macOS, setting C++ ctype locale seems to not propagate LC_CTYPE
    std::setlocale(LC_CTYPE, ctype.c_str());
}

void enable_utf8_encoding_globally() {
    // If default LC_CTYPE uses UTF8 codepage, then exit
    if (localname_has_utf8(std::setlocale(LC_CTYPE, nullptr)))
        return;

    // If the user-environment uses UTF8 codepage, create a copy of the default locale, then
    // copy the CTYPE part from the user-environment
    //
    // Ane empty locale "" represents the user-defined environment locale.On *nix, this is inherited
    // from the LANG, LC_ALL or LC_CTYPE environment variable.
    if (localname_has_utf8(std::locale("").name()))
        TRY_EXIT_ON_SUCCESS(set_ctype_globally(""));

    // Try different possibilities one by one until one works
    #if defined(_WIN32)
    // Use current locale, just change to UTF8 codepage (Windows only)
    TRY_EXIT_ON_SUCCESS(set_ctype_globally(".UTF-8"));
    #endif
    TRY_EXIT_ON_SUCCESS(set_ctype_globally("C.UTF-8"));
    TRY_EXIT_ON_SUCCESS(set_ctype_globally("en_US.UTF-8"));
    TRY_EXIT_ON_SUCCESS(set_ctype_globally("en_GB.UTF-8"));

    throw std::runtime_error("could not set a locale with UTF8 encoding");
}

}
