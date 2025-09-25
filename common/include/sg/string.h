#pragma once

#include "sg/export/common.h"
#include <string>

namespace sg::common {

/* Note, you MUST define a locale before manipulating or creating wide-strings. Otherwise, "C"
 * locale is used, which is not great.
 *
 * If the string was created in the current locale, use `std::locale::global(std::locale(""))` to
 * change local globally. If a specific locale was used for the string needed, you can specify that.
 *
 * FYI: calling `locale::global` changes the locale for both the C++ Standard Library and the C
 * Runtime Library. However, calling `setlocale` only changes the locale for the C Runtime Library;
 * the C++ Standard Library is not affected. (from
 * https://learn.microsoft.com/en-us/cpp/parallel/multithreading-and-locales?view=msvc-170)
 */

/* Converts a null-terminated multibyte character sequence to a wide-string. Thread safe. */
[[nodiscard]] SG_COMMON_EXPORT std::wstring to_wstring(const char* input);

/* Converts a string to a wide-string. Thread safe. */
[[nodiscard]] SG_COMMON_EXPORT std::wstring to_wstring(const std::string& input);

}