#include "crc32c_defs.h"

#ifdef HAVE_HARDWARE_CRC32_32
    #include <cstddef>
    #include <immintrin.h>

namespace {

/* Hardware-assited CRC-32c
 *
 * Basic 32-bit version
*/
inline uint32_t crc32c_hardware_32bit(const void * data, uint32_t no_of_bytes, uint32_t prev){
    auto R = prev;
    auto M = (const uint8_t*)data;

    while (no_of_bytes >=4) {
        R = _mm_crc32_u32(R, *(uint32_t*)M);
        no_of_bytes -=4;
        M += 4;
    };

    if (no_of_bytes>=2) {
        R = _mm_crc32_u16(R, *(uint16_t*)M);
        no_of_bytes -=2;
        M += 2;
    };

    if (no_of_bytes==1) {
        R = _mm_crc32_u8(R, *M);
        no_of_bytes -=1;
        M +=1;
    };

    return R;
}

}

#endif
