#pragma once

/* Don't define EXTERn_C, as in windows that is defined in <windows.h>.
 * Just write extern "C" fully, it's nto much longer. */

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
