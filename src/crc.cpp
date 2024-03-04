#include <sg/cpu.h>
#include <sg/crc.h>

#include "include/internal_crc32c.h"

#include <memory>
#include <boost/crc.hpp>

/* Tabular functions
 *
 * These are modified copies of those from https://github.com/komrad36/CRC
 *
 * Copyright (c) 2020 Kareem Omar (MIT license) */
namespace {

uint32_t crc32_tabular_1_byte(const void* M, uint32_t bytes, uint32_t prev, uint32_t* crcTable)
{
    const uint8_t* M8 = (const uint8_t*)M;
    uint32_t R = prev;
    for (uint32_t i = 0; i < bytes; ++i)
    {
        R = (R >> 8) ^ crcTable[(R ^ M8[i]) & 0xFF];
    }
    return R;
}
uint32_t crc32_tabular_2_bytes(const void* M, uint32_t bytes, uint32_t prev, uint32_t* crcTable)
{
    const uint16_t* M16 = (const uint16_t*)M;
    uint32_t R = prev;
    for (uint32_t i = 0; i < bytes >> 1; ++i)
    {
        R ^= M16[i];
        R = (R >> 16) ^
            crcTable[0 * 256 + uint8_t(R >> 8)] ^
            crcTable[1 * 256 + uint8_t(R >> 0)];
    }
    return R;
}
uint32_t crc32_tabular_4_bytes(const void* M, uint32_t bytes, uint32_t prev, uint32_t* crcTable)
{
    const uint32_t* M32 = (const uint32_t*)M;
    uint32_t R = prev;
    for (uint32_t i = 0; i < bytes >> 2; ++i)
    {
        R ^= M32[i];
        R = crcTable[0 * 256 + uint8_t(R >> 24)] ^
            crcTable[1 * 256 + uint8_t(R >> 16)] ^
            crcTable[2 * 256 + uint8_t(R >>  8)] ^
            crcTable[3 * 256 + uint8_t(R >>  0)];
    }
    return R;
}
uint32_t crc32_tabular_8_bytes(const void* M, uint32_t bytes, uint32_t prev, uint32_t* crcTable)
{
    const uint32_t* M32 = (const uint32_t*)M;
    uint32_t R = prev;
    while (bytes>=8)
    {
        R ^= *M32++;
        const uint32_t R2 = *M32++;
        R = crcTable[0 * 256 + uint8_t(R2 >> 24)] ^
            crcTable[1 * 256 + uint8_t(R2 >> 16)] ^
            crcTable[2 * 256 + uint8_t(R2 >> 8 )] ^
            crcTable[3 * 256 + uint8_t(R2 >> 0 )] ^
            crcTable[4 * 256 + uint8_t(R  >> 24)] ^
            crcTable[5 * 256 + uint8_t(R  >> 16)] ^
            crcTable[6 * 256 + uint8_t(R  >> 8 )] ^
            crcTable[7 * 256 + uint8_t(R  >> 0 )];
        bytes -= 8;
    }

    // Run 1-byte algorithm on any remaning bytes
    return crc32_tabular_1_byte(M32, bytes, R, crcTable);
}
uint32_t crc32_tabular_16_bytes(const void* M, uint32_t bytes, uint32_t prev, uint32_t* crcTable)
{
    const uint32_t* M32 = (const uint32_t*)M;
    uint32_t R = prev;
    while (bytes >=16)
    {
        R ^= *M32++;
        const uint32_t R2 = *M32++;
        const uint32_t R3 = *M32++;
        const uint32_t R4 = *M32++;
        R = crcTable[ 0 * 256 + uint8_t(R4 >> 24)] ^
            crcTable[ 1 * 256 + uint8_t(R4 >> 16)] ^
            crcTable[ 2 * 256 + uint8_t(R4 >> 8)] ^
            crcTable[ 3 * 256 + uint8_t(R4 >> 0)] ^
            crcTable[ 4 * 256 + uint8_t(R3 >> 24)] ^
            crcTable[ 5 * 256 + uint8_t(R3 >> 16)] ^
            crcTable[ 6 * 256 + uint8_t(R3 >> 8)] ^
            crcTable[ 7 * 256 + uint8_t(R3 >> 0)] ^
            crcTable[ 8 * 256 + uint8_t(R2 >> 24)] ^
            crcTable[ 9 * 256 + uint8_t(R2 >> 16)] ^
            crcTable[10 * 256 + uint8_t(R2 >> 8)] ^
            crcTable[11 * 256 + uint8_t(R2 >> 0)] ^
            crcTable[12 * 256 + uint8_t( R >> 24)] ^
            crcTable[13 * 256 + uint8_t( R >> 16)] ^
            crcTable[14 * 256 + uint8_t( R >> 8)] ^
            crcTable[15 * 256 + uint8_t( R >> 0)];
        bytes -= 16;
    }

    // Run 1-byte algorithm on any remaning bytes
    return crc32_tabular_8_bytes(M32, bytes, R, crcTable);
}
std::unique_ptr<uint32_t[]>  compute_tabular_method_tables(uint32_t polynomial)
{
    // We only do slicign up to 16 bits
    constexpr uint32_t crcMaxSliceBytes = 16;

    auto result = std::make_unique<uint32_t[]>(256 * crcMaxSliceBytes);
    auto pTbl = result.get();

    uint32_t i = 0;

    // initialize tbl0, the first 256 elements of pTbl,
    // using naive CRC to compute pTbl[i] = CRC(i)
    for (; i < 256; ++i)
    {
        uint32_t R = i;
        for (int j = 0; j < 8; ++j)
        {
            R = R & 1 ? (R >> 1) ^ polynomial : R >> 1;
        }
        pTbl[i] = R;
    }

    // initialize remaining tables by taking the previous
    // table's entry for i and churning through 8 more zero bits
    for (; i < crcMaxSliceBytes * 256; ++i)
    {
        const uint32_t R = pTbl[i - 256];
        pTbl[i] = (R >> 8) ^ pTbl[uint8_t(R)];
    }

    return result;
}
uint32_t crc32c_tabular(const void *data, uint32_t no_of_bytes, uint32_t initial_remainder,  uint32_t* crcTable){
    if (no_of_bytes >=16)
        return crc32_tabular_16_bytes(data, no_of_bytes, initial_remainder, crcTable);
    if (no_of_bytes >=8)
        return crc32_tabular_8_bytes(data, no_of_bytes, initial_remainder, crcTable);
    if (no_of_bytes >=4)
        return crc32_tabular_4_bytes(data, no_of_bytes, initial_remainder, crcTable);
    if (no_of_bytes >=2)
        return crc32_tabular_2_bytes(data, no_of_bytes, initial_remainder, crcTable);
    return crc32_tabular_1_byte(data, no_of_bytes, initial_remainder, crcTable);
}
}

namespace sg::checksum {

bool can_do_crc32c_hardware() {
#ifdef HAVE_HARDWARE_CRC32
    return true;
#else
    return false;
#endif
}

uint32_t crc32c(const void *data, uint32_t length) {
    /* Notes:
     *   - we invert (use ~) the result this is equivalent to XORing with 0xFFFFFFFF
     *   - the CRC32C initial remainder is 0xFFFFFFFFU */

    const uint32_t initial_remainder = 0xFFFFFFFFU;

#ifdef HAVE_HARDWARE_CRC32
    /* If available, use hardware assited version*/
    switch (sg::cpu::current_cpu_vendor()) {
    case sg::cpu::cpu_vendor::Amd:
        return ~_internal::crc32c_hardware_amd(data, length, initial_remainder);
    case sg::cpu::cpu_vendor::Intel:
        return ~_internal::crc32c_hardware_intel(data, length, initial_remainder);
    default:
        // For other vendors, go to tabular implementation
        break;
    }
#endif

    /* If can't use hardware, use 'Slicing' lookup tables*/
    static auto pTbl = compute_tabular_method_tables(0x82f63b78U);
    return ~crc32c_tabular(data, length, initial_remainder, pTbl.get());
}

uint32_t crc32(const void *data, uint32_t length) {
    static auto pTbl = compute_tabular_method_tables(0xEDB88320);
    return ~crc32c_tabular(data, length, 0xFFFFFFFFU, pTbl.get());
}

uint16_t crc16(const void *data, uint32_t length) {
    thread_local static boost::crc_16_type crc;
    crc.process_bytes(data, length);
    return crc();
}

} // namespace sg::checksum
