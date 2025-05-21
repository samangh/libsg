#pragma once

/* This header defines gettimeofday() in all plaforms.  In Windows, this
 * is done through a custom function. In other platforms it's done by
 * importing sys/time. */

#if __has_include(<sys/time.h>)
    #include <sys/time.h>
#elif defined(_WIN32)
    #include <Winsock2.h>
    #include <sg/export/common.h>
    #include <sg/extern_c.h>

/* providesgives the number of seconds and microseconds since the linux epoch
 *
 * Note:
 *  - the second parmeter is the time, which isn't used anymore.
 *  - this function is not for Win32 high precision timing purposes. See elapsed_time().
 */
extern "C" SG_COMMON_EXPORT int gettimeofday(struct timeval* tp, void* tzp);
#endif
