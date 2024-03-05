#pragma once

#include <cstdint>
#include <type_traits>
#include <vector>

#ifdef _MSC_VER
#include <stdlib.h> //Used in swap_bytes functins
#endif

#ifdef __APPLE__
#include <libkern/OSByteOrder.h> //Used in swap_bytes functins
#endif
namespace sg::bytes {

enum class Endianess { LittleEndian, BigEndian };

uint16_t to_uint16(const uint8_t *buff, Endianess endian = Endianess::LittleEndian);
uint32_t to_uint32(const uint8_t *buff, Endianess endian = Endianess::LittleEndian);
int32_t to_int32(const uint8_t *buff, Endianess endian = Endianess::LittleEndian);
uint64_t to_uint64(const uint8_t *buff, Endianess endian = Endianess::LittleEndian);
int64_t to_int64(const uint8_t *buff, Endianess endian = Endianess::LittleEndian);
double to_double(const uint8_t *buff, Endianess endian = Endianess::LittleEndian);

template <typename T>
std::vector<uint8_t>
to_bytes(T input,
         sg::bytes::Endianess endian,
         typename std::enable_if<std::is_integral<T>::value, std::nullptr_t>::type = nullptr) {
    /* Defined in header as this is a template, and so if defined in the
     * library we won't know about what types to compile for! */

    auto no_bytes = sizeof(T);
    std::vector<uint8_t> result(no_bytes);
    if (endian == sg::bytes::Endianess::BigEndian)
        for (unsigned int i = 0; i < no_bytes; i++)
            result[no_bytes - 1 - i] = (uint8_t)(input >> (i * 8));
    else
        for (unsigned int i = 0; i < no_bytes; i++)
            result[i] = (uint8_t)(input >> (i * 8));

    return result;
}

/**
 * @brief Casts the given object to an array of bytes.
 * @param input object
 * @return list of bytes
 */
template <typename T>
std::vector<uint8_t>
to_bytes(T input, typename std::enable_if<std::is_trivially_copyable<T>::value, bool> = true) {
    uint8_t *arr = reinterpret_cast<uint8_t *>(&input);
    std::vector<uint8_t> result(sizeof(input));

    for (int i = 0; i < sizeof(input); ++i)
        result[i] = arr[i];

    return result;
}

std::vector<uint8_t> to_bytes(double input, sg::bytes::Endianess endian);

/* Returns x with the order of the bytes reversed; for example, 0xaabb becomes 0xbbaa */
inline uint16_t swap_bytes(uint16_t x) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap16(x);
#elif defined(_MSC_VER)
    return _byteswap_ushort(x);
#elif defined(__APPLE__)
    OSSwapInt16(x);
#else
    /* The manual calculation code is from github:
     * Copyright 2020 github user jtbr, Released under MIT license */
    return (( x  >> 8 ) & 0xffu ) |
            (( x  & 0xffu ) << 8 );
#endif
}

/* Returns x with the order of the bytes reversed */
inline uint32_t swap_bytes(uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap32(x);
#elif defined(_MSC_VER)
    return _byteswap_ulong(x);
#elif defined(__APPLE__)
    OSSwapInt32(x);
#else
    return (x >> 24) |
           ((x >>  8) & 0x0000FF00) |
           ((x <<  8) & 0x00FF0000) |
            (x << 24);
#endif
}

/* Returns x with the order of the bytes reversed */
inline uint64_t swap_bytes(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap64(x);
#elif defined(_MSC_VER)
    return _byteswap_uint64(x);
#elif defined(__APPLE__)
    OSSwapInt64(x);
#else
    /* The manual calculation code is from github:
     * Copyright 2020 github user jtbr, Released under MIT license */
    return ((( x & 0xff00000000000000ull ) >> 56 ) |
            (( x & 0x00ff000000000000ull ) >> 40 ) |
            (( x & 0x0000ff0000000000ull ) >> 24 ) |
            (( x & 0x000000ff00000000ull ) >> 8  ) |
            (( x & 0x00000000ff000000ull ) << 8  ) |
            (( x & 0x0000000000ff0000ull ) << 24 ) |
            (( x & 0x000000000000ff00ull ) << 40 ) |
            (( x & 0x00000000000000ffull ) << 56 ));
#endif
}

} // namespace sg::bytes
