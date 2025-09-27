#pragma once

#include "sg/export/common.h"
#include <string>

namespace sg::common {

/* Note, you MUST define a locale be converting to/from wide strings. This is so that the system
 * knows how represent the Unicode code points in raw bytes (i.e. the codepage)
 *
 * I SUGGEST JUST CALLING `sg::common::set_global_utf8_codepage`
 *
 * It's recommended that you use the following locales
 *
 *   - Windows: ".utf8". This uses the current default Windows ANSI code page (ACP) for the locale
 *     and UTF-8 for the code page.
 *   - *nix: "C.utf8"
 *
 * If a user has already defined a locale and code page, then you can just use "" for locale. This
 * will mostly work fine on *nix systems, as on those systems this is inherited from the LANG,
 * LC_ALL or LC_CTYPE environment variable.
 *
 * To change the locale globally, say to C.utf8, do:
 *
 *   std::locale::global(std::locale("C.utf8"))
 *
 * FYI: calling `locale::global` changes the locale for both the C++ Standard Library and the C
 * Runtime Library. However, calling `setlocale` only changes the locale for the C Runtime Library;
 * the C++ Standard Library is not affected. (From
 * https://learn.microsoft.com/en-us/cpp/parallel/multithreading-and-locales?view=msvc-170)
 */

/** Ensures that the global locale uses UTF encoding.
 *
 * Does, in order:
 *
 *  - if the default codepge is UTF8, then does nothing
 *  - if the user has set a locale whose codepage is UTF8, then uses that
 *  - otherwise, manually sets the locale to use UTF8 codepage (".utf8" on Winodws, "C.utf8" on
 * other systems
 */
SG_COMMON_EXPORT void set_global_utf8_codepage();

/* Converts a null-terminated multibyte character sequence to a wide-string. Thread safe. */
[[nodiscard]] SG_COMMON_EXPORT std::wstring to_wstring(const char* input);

/* Converts a string to a wide-string. Thread safe. */
[[nodiscard]] SG_COMMON_EXPORT std::wstring to_wstring(const std::string& input);


}