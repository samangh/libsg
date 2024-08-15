#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#ifdef _MSC_VER
    #include <stdlib.h> //Used in swap_bytes functins
#endif

#ifdef __APPLE__
    #include <libkern/OSByteOrder.h> //Used in swap_bytes functins
#endif
namespace sg::bytes {

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
std::vector<uint8_t> to_bytes(T input, std::endian endian) {
    /* Defined in header as this is a template, and so if defined in the
     * library we won't know about what types to compile for! */

    auto no_bytes = sizeof(T);
    std::vector<uint8_t> result(no_bytes);
    if (endian == std::endian::big)
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
template <typename T,
          typename = typename std::enable_if<std::is_trivially_copyable<T>::value>::type>
std::vector<uint8_t> to_bytes(T input) {
    uint8_t* arr = reinterpret_cast<uint8_t*>(&input);
    std::vector<uint8_t> result(sizeof(input));

    for (int i = 0; i < sizeof(input); ++i)
        result[i] = arr[i];

    return result;
}

std::vector<uint8_t> to_bytes(double input, std::endian endian);

/* universal function for swapping bytes of a number */
template <std::integral T> constexpr T byteswap(T value) {
#if __cplusplus >= 202302L
    /* the following functions are already defined in C++23 */
    return std::byteswap(val);
#else
    static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
    auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    std::ranges::reverse(value_representation);
    return std::bit_cast<T>(value_representation);
#endif
}

#if __cplusplus < 202302L

/* Returns x with the order of the bytes reversed; for example, 0xaabb becomes 0xbbaa */
template <> constexpr uint16_t byteswap(uint16_t x) {
    #if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(x);
    #elif defined(_MSC_VER)
    return _byteswap_ushort(x);
    #elif defined(__APPLE__)
    OSSwapInt16(x);
    #else
    /* The manual calculation code is from github:
     * Copyright 2020 github user jtbr, Released under MIT license */
    return ((x >> 8) & 0xffu) | ((x & 0xffu) << 8);
    #endif
}

/* Returns x with the order of the bytes reversed */
template <> constexpr uint32_t byteswap(uint32_t x) {
    #if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(x);
    #elif defined(_MSC_VER)
    return _byteswap_ulong(x);
    #elif defined(__APPLE__)
    OSSwapInt32(x);
    #else
    return (x >> 24) | ((x >> 8) & 0x0000FF00) | ((x << 8) & 0x00FF0000) | (x << 24);
    #endif
}

/* Returns x with the order of the bytes reversed */
template <> constexpr uint64_t byteswap(uint64_t x) {
    #if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(x);
    #elif defined(_MSC_VER)
    return _byteswap_uint64(x);
    #elif defined(__APPLE__)
    OSSwapInt64(x);
    #else
    /* The manual calculation code is from github:
     * Copyright 2020 github user jtbr, Released under MIT license */
    return (((x & 0xff00000000000000ull) >> 56) | ((x & 0x00ff000000000000ull) >> 40) |
            ((x & 0x0000ff0000000000ull) >> 24) | ((x & 0x000000ff00000000ull) >> 8) |
            ((x & 0x00000000ff000000ull) << 8) | ((x & 0x0000000000ff0000ull) << 24) |
            ((x & 0x000000000000ff00ull) << 40) | ((x & 0x00000000000000ffull) << 56));
    #endif
}

#endif

template <std::integral T>
T to_integral(const uint8_t* buff, std::endian src_endian = std::endian::native) {
    T val;
    memcpy(&val, buff, sizeof(T));

    if (std::endian::native != src_endian)
        return sg::bytes::byteswap(val);
    return val;
}

double to_double(const uint8_t* buff, std::endian endian = std::endian::native);

} // namespace sg::bytes
