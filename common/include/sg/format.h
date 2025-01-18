#pragma once

#include "bytes.h"

#include <fmt/compile.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <ranges>
#include <string>
#include <type_traits>

namespace sg::format {

/*!
 * \brief Convert container items to hex string.
 *        Item type must be trivially covertible to std::byte.
 *
 * \param range Tnput range
 * \param separator The sperator between each byte
 * \return The hex representation as string, without the starting 0x.
 */
template <typename RangeT>
requires(std::ranges::range<RangeT>&&
             std::is_convertible_v<std::ranges::range_value_t<RangeT>, std::byte>)
[[nodiscard]] std::string to_hex(const RangeT& range, std::string_view separator = "") {
    // limit type to std::byte, so that we can use fixed {:0X} representation with padding if
    // required.
    return fmt::format("{:02X}", fmt::join(range, separator));
}

/*!
 * \brief Convert container items to hex string.
 *        Item type must be trivially covertible to std::byte.
 *
 * \param range Tnput range
 * \param separator The sperator between each byte
 * \return The hex representation as string, without the starting 0x.
 */
template <std::input_iterator BeginT, std::sentinel_for<BeginT> SentinelT>
requires(std::is_convertible_v<std::iter_value_t<BeginT>, std::byte>)

[[nodiscard]] std::string to_hex(const BeginT begin, const SentinelT end, std::string_view separator = "") {
    return fmt::format("{:02X}", fmt::join(begin, end, separator));
}

/*!
 * \brief Convert trivial object to hex string.
 *
 *        Item is first trivially converted to bytes using sg::bytes::to_bytes(item).
 *        Will create at least one copy of the object.
 *
 * \param range Tnput range
 * \param separator The sperator between each byte
 * \return The hex representation as string, without the starting 0x.
 */
template <typename T>
[[nodiscard]] std::string to_hex(const T& item, std::string_view separator = "") {
    // return fmt::format("{:0{}X}", sizeof(T), item);
    return to_hex(sg::bytes::to_bytes(item), separator);
}

}  // namespace sg::format
