#pragma once
#include <sg/export/common.h>

/* in clang/gcc, exception classes must always be exported (e.g. visible), this is because
 * Exception catching of a user defined type in a binary other than the one which threw the
 * exception requires a typeinfo lookup.
 *
 * You also need to do this for classes that are used as operands of @c dynamic_cast, but in that
 * cause
 *
 * We define VISIBLE (instead of SG_COMMON_EXPORT) because SG_CREATE_EXCEPTION generator macor could
 * be used by another library to create their own custom exceptions, and that case it  would expand
 * to
 * __declspec(dllimport) on Windows, which would be * incorrect because the external library _is_
 * the one defining the class (it's just the macro expansion is wrong).
 *
 * You also need to do export classes that are used as operands of @c dynamic_cast, but in that
 *  case you almost always want SG_COMMON_EXPORT.
 */

#if defined _WIN32 || defined __CYGWIN__
#define VISIBLE
#else
#define VISIBLE __attribute__((visibility("default")))
#endif
