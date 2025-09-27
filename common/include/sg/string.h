#pragma once

#include "sg/export/common.h"
#include <string>

namespace sg::string {

/* Note, you MUST define a locale be converting to/from wide strings. This is so that the system
 * knows how represent the Unicode code points in raw bytes (i.e. the codepage)
 *
 * I SUGGEST JUST CALLING `sg::locale::use_utf8_encoding()`.
 * To change the locale globally, say to C.utf8, do:
 *
 *   std::locale::global(std::locale("C.utf8"))
 */

/* Converts a null-terminated multibyte character sequence to a wide-string. Thread safe. */
[[nodiscard]] SG_COMMON_EXPORT std::wstring to_wstring(const char* input);

/* Converts a string to a wide-string. Thread safe. */
[[nodiscard]] SG_COMMON_EXPORT std::wstring to_wstring(const std::string& input);


}