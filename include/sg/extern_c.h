#pragma once

/* If in Windows, EXTERN_C is already defined in <windows.h>, use that
 * instead. Else define our own. */

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#ifndef EXTERN_C
    #ifdef __cplusplus
        #define EXTERN_C extern "C"
    #else
        #define EXTERN_C
    #endif
#endif

#ifndef EXTERN_C_BEGIN
    #ifdef __cplusplus
        #define EXTERN_C_BEGIN extern "C" {
    #else
        #define EXTERN_C_BEGIN
    #endif
#endif

#ifndef EXTERN_C_END
    #ifdef __cplusplus
        #define EXTERN_C_END }
    #else
        #define EXTERN_C_END
    #endif
#endif
