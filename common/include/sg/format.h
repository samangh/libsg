#pragma once

#include "bytes.h"
#include "ranges.h"

#include <fmt/compile.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <ranges>
#include <string>
#include <type_traits>

namespace sg::format {

/*!
 * \brief Convert container items to hex string.
 *
 *        Note: will do at least two copy operations.
 * \param rangeT input range
 * \param separator The sperator between each byte
 * \return The hex representation as string, without the starting 0x.
 */
template <typename RangeT>
    requires(std::ranges::contiguous_range<RangeT> &&
             std::is_trivially_copyable_v<std::ranges::range_value_t<RangeT>> &&
             std::has_unique_object_representations_v<std::ranges::range_value_t<RangeT>>)
[[nodiscard]] std::string to_hex(const RangeT& range, std::string_view separator = "") {
    auto result = sg::bytes::to_bytes(range);
    return fmt::format("{:02X}", fmt::join(std::begin(result), std::end(result), separator));
}

 /**
 * \brief Convert container items to hex string.
 *
 *        Note: will do at least two copy operations.
 * \param separator The sperator between each byte
 * \return The hex representation as string, without the starting 0x.
 */

template <std::input_iterator BeginT, std::sentinel_for<BeginT> SentinelT>
    requires(std::is_trivially_copyable_v<std::iter_value_t<BeginT>> &&
             std::has_unique_object_representations_v<std::iter_value_t<BeginT>>)
[[nodiscard]] std::string to_hex(const BeginT begin,
                                 const SentinelT end,
                                 std::string_view separator = "") {
    auto size = (end - begin) * sizeof(std::iter_value_t<BeginT>);

    std::vector<std::byte> result;
    result.reserve(size);
    for (BeginT it = begin; it != end; it++) sg::ranges::append(result, sg::bytes::to_bytes(*it));

    return fmt::format("{:02X}", fmt::join(std::begin(result), std::end(result), separator));
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
    requires(std::is_trivially_copyable_v<T> && std::has_unique_object_representations_v<T> &&
             !std::is_pointer_v<T> && !std::ranges::range<T>)
[[nodiscard]] std::string to_hex(const T& item, std::string_view separator = "") {
    auto result = sg::bytes::to_bytes(item);
    return fmt::format("{:02X}", fmt::join(std::begin(result), std::end(result), separator));

}

/* Fast to string conversion */
template <typename T>
[[nodiscard]] inline std::string to_string(const T& val) {
    return (fmt::to_string(val));
}

}  // namespace sg::format
