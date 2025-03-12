#include "crc32c_defs.h"

#ifdef HAVE_HARDWARE_CRC32_ARMV7
    #include <arm_acle.h>
    #include <arm_neon.h>

namespace {

inline uint32_t crc32c_hardware_armv7(const void * data, uint32_t no_of_bytes, uint32_t prev){
    auto R = prev;
    auto M = (const uint8_t*)data;

    while (no_of_bytes >=8) {
        R = __crc32cd(R, *((uint64_t*)M));
        no_of_bytes -= 8;
        M+=8;
    }

    if (no_of_bytes >=4) {
        R = __crc32cw(R, *((uint32_t*)M));
        no_of_bytes -= 4;
        M+=4;
    }

    if (no_of_bytes >=2) {
        R = __crc32ch(R, *((uint16_t*)M));
        no_of_bytes -= 2;
        M+=2;
    }

    if (no_of_bytes ==1) {
        R = __crc32cb(R, *((uint8_t*)M));
        no_of_bytes -= 1;
        M+=1;
    }

    return R;
}

}

#endif

