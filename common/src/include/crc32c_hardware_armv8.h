#include "crc32c_defs.h"

/* **************************************************************************
 * Modified copy of implementation from google/crc32c repo, which is under a
 * BSD-3-Clause license. See LICENCES/google-crc32c.md
 * **************************************************************************
 *
 * Copyright 2017, The CRC32C Authors.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 *
 * * Neither the name of Google Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_HARDWARE_CRC32C_ARMV8
#include <arm_acle.h>
#include <arm_neon.h>

namespace {

#define KBYTES 1032
#define SEGMENTBYTES 256

// compute 8bytes for each segment parallelly
#define CRC32C32BYTES(P, IND)                                                     \
do {                                                                              \
        crc1 = __crc32cd(crc1, *((uint64_t*)((P) + SEGMENTBYTES * 1 + (IND)*8))); \
        crc2 = __crc32cd(crc2, *((uint64_t*)((P) + SEGMENTBYTES * 2 + (IND)*8))); \
        crc3 = __crc32cd(crc3, *((uint64_t*)((P) + SEGMENTBYTES * 3 + (IND)*8))); \
        crc0 = __crc32cd(crc0, *((uint64_t*)((P) + SEGMENTBYTES * 0 + (IND)*8))); \
} while (0);

// compute 8*8 bytes for each segment parallelly
#define CRC32C256BYTES(P, IND)      \
do {                              \
        CRC32C32BYTES((P), (IND)*8 + 0) \
        CRC32C32BYTES((P), (IND)*8 + 1) \
        CRC32C32BYTES((P), (IND)*8 + 2) \
        CRC32C32BYTES((P), (IND)*8 + 3) \
        CRC32C32BYTES((P), (IND)*8 + 4) \
        CRC32C32BYTES((P), (IND)*8 + 5) \
        CRC32C32BYTES((P), (IND)*8 + 6) \
        CRC32C32BYTES((P), (IND)*8 + 7) \
} while (0);

// compute 4*8*8 bytes for each segment parallelly
#define CRC32C1024BYTES(P)       \
do {                             \
        CRC32C256BYTES((P), 0)   \
        CRC32C256BYTES((P), 1)   \
        CRC32C256BYTES((P), 2)   \
        CRC32C256BYTES((P), 3)   \
        (P) += 4 * SEGMENTBYTES; \
} while (0)


inline uint32_t crc32c_hardware_armv8(const void * data, uint32_t no_of_bytes, uint32_t prev){
    auto M = (const uint8_t*)data;
    auto crc=prev;

    uint32_t crc0, crc1, crc2, crc3;
    uint64_t t0, t1, t2;

    // k0=CRC(x^(3*SEGMENTBYTES*8)), k1=CRC(x^(2*SEGMENTBYTES*8)),
    // k2=CRC(x^(SEGMENTBYTES*8))
    const poly64_t k0 = 0x8d96551c, k1 = 0xbd6f81f8, k2 = 0xdcb17aa4;

    while (no_of_bytes >= KBYTES) {
        crc0 = crc;
        crc1 = 0;
        crc2 = 0;
        crc3 = 0;

        // Process 1024 bytes in parallel.
        CRC32C1024BYTES(M);

        // Merge the 4 partial CRC32C values.
        t2 = (uint64_t)vmull_p64(crc2, k2);
        t1 = (uint64_t)vmull_p64(crc1, k1);
        t0 = (uint64_t)vmull_p64(crc0, k0);
        crc = __crc32cd(crc3, *((uint64_t*)(M)));
        M += sizeof(uint64_t);
        crc ^= __crc32cd(0, t2);
        crc ^= __crc32cd(0, t1);
        crc ^= __crc32cd(0, t0);

        no_of_bytes -= KBYTES;
    }

    while (no_of_bytes >= 8) {
        crc = __crc32cd(crc, *((uint64_t*)(M)));
        M += 8;
        no_of_bytes -= 8;
    }

    if (no_of_bytes & 4) {
        crc = __crc32cw(crc, *((uint32_t*)(M)));
        M += 4;
    }

    if (no_of_bytes & 2) {
        crc = __crc32ch(crc, *((uint16_t*)(M)));
        M += 2;
    }

    if (no_of_bytes & 1) {
        crc = __crc32cb(crc, *M);
    }

    return crc;
}

}

#endif

