#pragma once

#include <sg/export/common.h>
#include <string>

namespace sg::locale {

/** Ensures that the global locale uses UTF encoding for character functions. The modifications
 * remain in effect and influences the execution of all locale-sensitive library functions.
 *
 * Note that this only sets the "the character classification category" of the C (LC_CTYPE)
 * and C++ (std::locale::ctype) locales.
 *
 * Tries, in order:
 *
 *  - if the default codepage is UTF8, then does nothing
 *  - if the user has set a locale whose codepage is UTF8, then uses that
 *  - otherwise, manually sets the LC_CTYPE part of the locale to use UTF8 codepage by trying
 *    ("C.utf8", "en_US.UTF-8", "en_GB.UTF-8")
 */
SG_COMMON_EXPORT void enable_utf8_encoding_globally();

/** Sets the global locale for character functions. The modifications
 * remain in effect and influences the execution of all locale-sensitive library functions.
 *
 * Note that this only sets the "the character classification category" of the C (LC_CTYPE)
 * and C++ (std::locale::ctype) locales.
 *
 * To change everything, you can run <tt>std::locale::global(std::locale("C.utf8"))</tt>, but this is a
 * <em>very very bad</em> idea. See https://unix.stackexchange.com/questions/149111
 */
SG_COMMON_EXPORT void set_ctype_globally(const std::string& ctype);

}