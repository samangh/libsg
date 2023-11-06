#pragma once

/* This header defines gettimeofday() in all plaforms.  In Windows, this
 * is done through a custom function. In other pplatforms it's done by
 * importing sys/time. */

#ifdef _WIN32
    #include <Winsock2.h>
    #include <sg/export/sg_common.h>
    #include <sg/extern_c.h>

EXTERN_C SG_COMMON_EXPORT int gettimeofday(struct timeval *tp, struct timezone *tzp);
#else
    #include <sys/time.h>
#endif
