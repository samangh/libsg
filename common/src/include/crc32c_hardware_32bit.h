#include "crc32c_defs.h"

#ifdef HAVE_HARDWARE_CRC32_32
    #include <cstddef>
    #include <immintrin.h>

namespace {

inline uint32_t crc32c_32bit_1byte(const void *data,
                            std::size_t no_of_bytes,
                            uint32_t remainder) {
    auto R = remainder;
    auto M = (const uint8_t*)data;
    for (uint32_t i = 0; i < no_of_bytes; ++i) {
        R = _mm_crc32_u8(R, M[i]);
    }
    return R;
}



/* Hardware-assited CRC-32c
 *
 * Basic 32-bit version
 */
inline uint32_t crc32c_hardware_32bit(const void * data, uint32_t no_of_bytes, uint32_t prev){
    auto R = prev;
    auto M = (const uint8_t*)data;
    uint32_t i = 0;
    while (i + 4 < no_of_bytes) {
        R = _mm_crc32_u32(R, *((uint32_t*)&M[i]));
        i += 4;
    }

    return crc32c_32bit_1byte(&M[i], no_of_bytes - i, R);
}

}

#endif
