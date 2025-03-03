#pragma once

#include <sg/environment.h>

/* Optimized hardware assisted CRC32C needs SSE4.2, CRC32 and CLMUL support from the CPU  */
#if defined(CPU_SUPPORTS_SSE42) && defined(CPU_SUPPORTS_CRC32) && defined(CPU_SUPPORTS_CLMUL)
    #define HAVE_HARDWARE_CRC32 true
#endif

