#pragma once

#include "bytes.h"
#include "concepts.h"
#include "ranges.h"

#include <fmt/core.h>
#include <fmt/compile.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/chrono.h>
#include <ranges>
#include <string>
#include <type_traits>

namespace sg::format {

class Default final {};

/*************************************************************************************************
 * compile-time format string forwarder
 *************************************************************************************************/

/* Allows for passing a string a template argument. Useful for passing fmt format-strings across
 * functions.
 *
 * Note: if you want to generate a fmt format-string during runtime, use fmt::runtime(...).
 *
 * See https://stackoverflow.com/questions/68675303/how-to-create-a-function-that-forwards-its-arguments-to-fmtformat-keeping-the
 */
template <std::size_t N>
struct static_string {
    char str[N] {};
    constexpr static_string(const char (&s)[N]) {
        std::ranges::copy(s, str);
    }
};

template <static_string fmt, typename... Args>
auto format(Args&&... args) {
    return std::format(fmt.str, std::forward<Args>(args)...);
}

template <static_string fmt, typename... Args>
auto format_compiled(Args&&... args) {
    return std::format(FMT_COMPILE(fmt.str), std::forward<Args>(args)...);
}

/*************************************************************************************************/


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
requires(!sg::concepts::is_time_point_v<T>)
[[nodiscard]] inline std::string to_string(const T& val) {
    return (fmt::to_string(val));
}

/*************************************************************************************************
 * time_point to string conversion
 *************************************************************************************************/

/** Converts a time_point to text, with a given format.
 *
 * @tparam fmt fmt-style format string (e.g. "{:%F %T%z}")
 * @tparam TDuration resolution of time duration, leave as default to use same resolution as the time
 * point. Alternative pass a duration (e.g. std::chrono::seconds)
 * @param time_point time_point (e.g. std::chrono::system_clock::now())
 * @return
 */
template <static_string fmt, typename TDuration = Default>
[[nodiscard]] std::string to_string(const sg::concepts::is_time_point_v auto& time_point) {
    if constexpr (std::is_same_v<TDuration, Default>) {
        return fmt::format(fmt.str, time_point);
    } else
        return fmt::format(fmt.str, std::chrono::round<TDuration>(time_point));
}

/** Formats a time_point to a string, not showing the timezone. Resolution to seconds.
 *
 * Example result: "2025-01-01 01:02:01"
 */
template <typename TTimePoint>
[[nodiscard]] std::string to_string(const TTimePoint& time_point)
{
    return to_string<"{:%F %T}", std::chrono::seconds>(time_point);
}

/** Converts time_point to ISO 8061 format, with second precision
 *
 * Example result: "2025-01-01T01:02:01+0000"
 */
template <typename TTimePoint>
[[nodiscard]] std::string to_string_iso(const TTimePoint& time_point)
{
    return to_string<"{:%FT%T%z}", std::chrono::seconds>(time_point);
}


/*************************************************************************************************/

}  // namespace sg::format
