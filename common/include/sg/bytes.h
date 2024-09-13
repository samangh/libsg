#pragma once

#include "sg/export/common.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace sg::bytes {

/* universal function for swapping bytes of a number.
 *
 * This is optimised well by clang/gcc, not as well on MSVC. */
template <std::integral T>
[[nodiscard]] constexpr T byteswap(T value) {
#ifdef __cpp_lib_byteswap
    return std::byteswap(val);
#else
    static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
    auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    std::ranges::reverse(value_representation);
    return std::bit_cast<T>(value_representation);
#endif
}

/* Converts an object to bytes */
template <typename T>
  requires(std::is_trivially_copyable_v<T> &&
           std::has_unique_object_representations_v<T>)
[[nodiscard]] constexpr std::array<std::byte, sizeof(T)> to_bytes(T input) {
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(input);
}

/* Converts an integer objectg to bytes, modified to match a target endianess*/
template <std::integral T>
[[nodiscard]] std::vector<std::byte> to_bytes(T input, std::endian dest_endian) {
    if (dest_endian != std::endian::native)
      input = byteswap(input);

    return to_bytes(input);
}

template <std::integral T>
[[nodiscard]] T to_integral(const std::byte* buff, std::endian src_endian = std::endian::native) {
    T val;
    memcpy(&val, buff, sizeof(T));

    if (std::endian::native != src_endian)
        return sg::bytes::byteswap(val);
    return val;
}

[[nodiscard]] SG_COMMON_EXPORT double to_double(const std::byte* buff,
                                                std::endian src_endian = std::endian::native);

[[nodiscard]] SG_COMMON_EXPORT std::array<std::byte, sizeof(double)> to_bytes(
    double input, std::endian endian = std::endian::native);

} // namespace sg::bytes
