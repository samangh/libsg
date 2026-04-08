#pragma once

#include "ranges.h"
#include "debug.h"

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
    return std::byteswap(value);
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

/* Converts an integer object to bytes, modified to match a target endianness */
template <typename T>
    requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
[[nodiscard]] std::array<std::byte, sizeof(T)> to_bytes(T input, std::endian dstEndian) {
    /* byteswap is more efficient, so where possible we do byteswap first following by bitcast */
    if constexpr (std::is_integral_v<T>) {
        if (dstEndian != std::endian::native)
            input = byteswap(input);
        return to_bytes(input);
    } else if constexpr (std::is_floating_point_v<T>) {
        auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(input);
        if (std::endian::native != dstEndian)
            std::ranges::reverse(bytes);
        return bytes;
    }
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

/****************************** to_numeric / to_object ******************************/

template <typename T>
    requires(std::is_trivially_copyable_v<T> && !std::is_pointer_v<T> && !std::ranges::range<T>)
[[nodiscard]] T to_object(std::byte* buff) {
    T val;
    memcpy(&val, buff, sizeof(T));
    return val;
}

template <typename T>
    requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
[[nodiscard]] T to_numeric(const std::byte* buff, std::endian src_endian = std::endian::native) {
    if constexpr (std::is_integral_v<T>) {
        T val;
        memcpy(&val, buff, sizeof(T));

        if (std::endian::native != src_endian)
            return sg::bytes::byteswap(val);
        return val;
    } else if constexpr (std::is_floating_point_v<T>) {
        T val;

        if (std::endian::native == src_endian)
            memcpy(&val, buff, sizeof(T));
        else
        {
            auto ptr = (std::byte*)&val;
            const auto size = sizeof(T);
            for(size_t i=0; i < size; ++i)
                ptr[i]=buff[size-i-1];
        }
        return val;
    } else
        SG_THROW(std::invalid_argument, "can't convert to numeric");
}


template <std::integral T, typename It>
    requires(std::contiguous_iterator<It> && std::is_same_v<std::iter_value_t<It>, std::byte>)
[[nodiscard]] T to_numeric(const It it, const It end, std::endian src_endian = std::endian::native) {
    if (it + sizeof(T) > end)
        SG_THROW(std::runtime_error, "not enough bytes to convert the required item");

    return  to_numeric<T>(&*it,src_endian);
}

/****************************** to_integral_and_advance_iterator ******************************/

template <typename T>
    requires(std::is_trivially_copyable_v<T> &&
             !std::is_pointer_v<T> && !std::ranges::range<T>)
[[nodiscard]] T to_object_and_advance_ptr(std::byte** buff) {
    T val =to_object<T>(*buff);
    *buff = *buff + sizeof(T);
    return val;
}


template <std::integral T>
[[nodiscard]] T to_numeric_and_advance_ptr(std::byte** buff, std::endian src_endian = std::endian::native) {
    T val = to_numeric<T>( *buff, src_endian);
    *buff = *buff + sizeof(T);
    return val;
}

template <std::integral T, typename It>
    requires(std::contiguous_iterator<It> && std::is_same_v<std::iter_value_t<It>, std::byte>)
[[nodiscard]] T to_numeric_and_advance_iterator(It& it, const It end, std::endian src_endian = std::endian::native) {
    T val = to_numeric<T>(it, end, src_endian);
    it+=sizeof(T);
    return val;
}


} // namespace sg::bytes
