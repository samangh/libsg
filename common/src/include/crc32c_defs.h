#pragma once

#include <sg/environment.h>

/* Optimized hardware assistance
 *
 * Our 64-bit code requires: SSE4.2 and CLUL
 * Our 32-bit code required: SSE4.2
 */
#if defined(ENV_64BIT) && defined(CPU_SUPPORTS_SSE42) && defined(CPU_SUPPORTS_CLMUL)
    #define HAVE_HARDWARE_CRC32C_64 1
#endif

#if defined(CPU_SUPPORTS_SSE42)
    #define HAVE_HARDWARE_CRC32C_32 1
#endif

#if defined(CPU_SUPPORTS_ARM_CRC) && defined(CPU_SUPPORTS_ARM_AES)
    #define HAVE_HARDWARE_CRC32C_ARMV8
#elif defined(CPU_SUPPORTS_ARM_CRC)
    #define HAVE_HARDWARE_CRC32C_ARMV7
#endif
