#pragma once

#include <cstdint>

/* Optimized hardware assisted CRC32C needs SSE4.2, CRC32 and CLMUL support from the CPU  */
#if defined(CPU_SUPPORTS_SSE42) && defined(CPU_SUPPORTS_CRC32) && defined(CPU_SUPPORTS_CLMUL)
    #define HAVE_HARDWARE_CRC32 true

namespace _internal {

uint32_t crc32c_hardware_amd(const void *M, uint32_t bytes, uint32_t prev);
uint32_t crc32c_hardware_intel(const void *M, uint32_t bytes, uint32_t prev);

} // namespace _internal

#endif

