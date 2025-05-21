/* The gettimeofday(...) was copied from the PostgreSQL source, with
 * the following licence:
 *
 * win32gettimeofday.c
 *    Win32 gettimeofday() replacement
 *
 * src/port/win32gettimeofday.c
 *
 * Copyright (c) 2003 SRA, Inc.
 * Copyright (c) 2003 SKC, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose, without fee, and without a
 * written agreement is hereby granted, provided that the above
 * copyright notice and this paragraph and the following two
 * paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT,
 * INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
 * LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
 * DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS
 * IS" BASIS, AND THE AUTHOR HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#if !__has_include(<sys/time.h>) && defined(_WIN32)

    #include <sg/gettimeofday.h>

    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #include <Windows.h>
    #include <cstdint>

/* FILETIME of Jan 1 1970 00:00:00, the PostgreSQL epoch */
static constexpr uint64_t epoch = ((uint64_t)116444736000000000ULL);

/*
 * FILETIME represents the number of 100-nanosecond intervals since
 * January 1, 1601 (UTC).
 */
#define FILETIME_UNITS_PER_SEC 10000000L
#define FILETIME_UNITS_PER_USEC 10

int gettimeofday(struct timeval* tp, void*) {
    /*
     * timezone information is stored outside the kernel so tzp isn't used anymore.
     *
     * Note: this function is not for Win32 high precision timing purposes. See
     * elapsed_time().
     */

    FILETIME file_time;
    ULARGE_INTEGER ularge;

    #if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    GetSystemTimePreciseAsFileTime(&file_time);
    #else
    SYSTEMTIME system_time;
    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    #endif

    ularge.LowPart = file_time.dwLowDateTime;
    ularge.HighPart = file_time.dwHighDateTime;

    tp->tv_sec = (long)((ularge.QuadPart - epoch) / FILETIME_UNITS_PER_SEC);
    tp->tv_usec =
        (long)(((ularge.QuadPart - epoch) % FILETIME_UNITS_PER_SEC) / FILETIME_UNITS_PER_USEC);

    return 0;
}
#endif
