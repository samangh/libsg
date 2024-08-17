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
template <> constexpr int16_t byteswap(int16_t x) {
    return static_cast<int16_t>(byteswap(static_cast<uint16_t>(x)));
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
template <> constexpr int32_t byteswap(int32_t x) {
    return static_cast<int32_t>(byteswap(static_cast<uint32_t>(x)));
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
template <> constexpr int64_t byteswap(int64_t x) {
    return static_cast<int64_t>(byteswap(static_cast<uint64_t>(x)));
}

#endif

/* Converts an object to bytes */
template <typename T>
  requires(std::is_trivially_copyable_v<T> &&
           std::has_unique_object_representations_v<T>)
constexpr std::array<std::byte, sizeof(T)> to_bytes(T input) {
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(input);
}

/* Converts an integer objectg to bytes, modified to match a target endianess*/
template <std::integral T>
std::vector<std::byte> to_bytes(T input, std::endian dest_endian) {
    if (dest_endian != std::endian::native)
      input = byteswap(input);

    return to_bytes(input);
}

template <std::integral T>
T to_integral(const std::byte* buff, std::endian src_endian = std::endian::native) {
    T val;
    memcpy(&val, buff, sizeof(T));

    if (std::endian::native != src_endian)
        return sg::bytes::byteswap(val);
    return val;
}

double to_double(const uint8_t* buff, std::endian endian = std::endian::native);
std::vector<uint8_t> to_bytes(double input, std::endian endian);

} // namespace sg::bytes
