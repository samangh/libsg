#pragma once

#include "sg/export/common.h"
#include "ranges.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>
#include <stdexcept>

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

/**
* @brief returns byte representation of a signle object
*
*        Note: wil do a copy
*/
template <typename T>
    requires(std::is_trivially_copyable_v<T> && std::has_unique_object_representations_v<T> &&
             !std::is_pointer_v<T> && !std::ranges::range<T>)
[[nodiscard]] constexpr std::array<std::byte, sizeof(T)> to_bytes(const T& input) {
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(input);
}

 /**
 * @brief returns byte representation of a contiguous range of objects
 *
 *        Note: wil do a copy
 */
template <typename RangeT>
    requires(std::ranges::contiguous_range<RangeT> &&
             std::is_trivially_copyable_v<std::ranges::range_value_t<RangeT>> &&
             std::has_unique_object_representations_v<std::ranges::range_value_t<RangeT>>)
[[nodiscard]] constexpr std::vector<std::byte> to_bytes(const RangeT& range) {
    auto size = std::size(range) * sizeof(std::ranges::range_value_t<RangeT>);

    std::vector<std::byte> result;
    result.reserve(size);
    std::ranges::for_each(
        range, [&result](const auto& item) { sg::ranges::append(result, to_bytes(item)); });

    return result;
}

/* Converts an integer objectg to bytes, modified to match a target endianess*/
template <std::integral T>
[[nodiscard]] std::vector<std::byte> to_bytes(T input, std::endian dest_endian) {
    if (dest_endian != std::endian::native)
      input = byteswap(input);

    return to_bytes(input);
}

/****************************** to_integral ******************************/

template <std::integral T>
[[nodiscard]] T to_integral(const std::byte* buff, std::endian src_endian = std::endian::native) {
    T val;
    memcpy(&val, buff, sizeof(T));

    if (std::endian::native != src_endian)
        return sg::bytes::byteswap(val);
    return val;
}

template <std::integral T, typename It>
    requires(std::contiguous_iterator<It> && std::is_same_v<std::iter_value_t<It>, std::byte>)
[[nodiscard]] T to_integral(const It it, const It end, std::endian src_endian = std::endian::native) {
    if (it+ sizeof(T) > end)
        throw std::runtime_error("not enough bytes to convert the required item");

    T val;
    memcpy(&val, &*it, sizeof(T));

    if (std::endian::native != src_endian)
        return sg::bytes::byteswap(val);
    return val;
}

/****************************** to_integral_and_advance_iterator ******************************/

template <std::integral T>
[[nodiscard]] T to_integral_and_advance_ptr(std::byte** buff, std::endian src_endian = std::endian::native) {
    T val;
    memcpy(&val, *buff, sizeof(T));

    if (std::endian::native != src_endian)
        return sg::bytes::byteswap(val);
    *buff = *buff + sizeof(T);
    return val;
}

template <std::integral T, typename It>
    requires(std::contiguous_iterator<It> && std::is_same_v<std::iter_value_t<It>, std::byte>)
[[nodiscard]] T to_integral_and_advance_iterator(It& it, const It end, std::endian src_endian = std::endian::native) {
    if (it+ sizeof(T) > end)
        throw std::runtime_error("not enough bytes to convert the required item");

    T val;
    memcpy(&val, &*it, sizeof(T));

    if (std::endian::native != src_endian)
        return sg::bytes::byteswap(val);

    it+=sizeof(T);

    return val;
}

[[nodiscard]] SG_COMMON_EXPORT double to_double(const std::byte* buff,
                                                std::endian src_endian = std::endian::native);

[[nodiscard]] SG_COMMON_EXPORT std::array<std::byte, sizeof(double)> to_bytes(
    double input, std::endian endian = std::endian::native);

} // namespace sg::bytes
