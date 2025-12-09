#pragma once

#include <chrono>
#include <version>
#if __cpp_lib_chrono < 201907L
    #include "internal/date/date.h"
#endif

namespace sg::time {

/** Converts a date-time string, in a given format, to a time-point.
 *
 * @tparam TTimePoint Time point time. Leave as default, unless you know the implications.
 * @param input  Input string (e.g. "2025-01-01 01:02:00", "%F %T")
 * @param format Input format (e.g. "%F %T"), see
 *               https://en.cppreference.com/w/cpp/chrono/parse.html
 * @return
 */
template <typename TTimePoint = std::chrono::sys_time<std::chrono::system_clock::duration>>
[[nodiscard]] TTimePoint from_string(const std::string& input, const std::string& format) {
    TTimePoint t;
    std::istringstream is{input};

    #if (__cpp_lib_chrono >= 201907L)
        std::chrono::from_stream(is, format.c_str(), t);
    #else
        date::from_stream(is, format.c_str(), t);
    #endif

    if (is.fail())
        throw std::invalid_argument("could not parse string to a time_point");

    return t;
}

/** Converts from epoch to a time point.
 *
 * @tparam TClock Clock to use, leave as default unless you want to use non-POSIX epoch.
 * @param epochSeconds Epoch is seconds
 * @return time-point in the desired clock
 */
template <typename TClock = std::chrono::system_clock>
std::chrono::time_point<TClock> from_epoch(double epochSeconds) {
    /* create a duration, assuming the epoch is in seconds */
    std::chrono::duration<double, std::ratio<1>> sinceEpoch(epochSeconds);

    /* cast to duration of the clock */
    auto durCast = std::chrono::duration_cast<typename TClock::duration>(sinceEpoch);

    return std::chrono::sys_time(durCast);
}

/** Converts time-pint to an epoch
 *
 * @tparam TTimePoint Type of time-point, you usually don't have to specify this.
 * @param timeIn time-point (e.g. generated from std::chrono::system_clock::now())
 * @return Epoch in seconds.
 */
template <typename TTimePoint>
[[nodiscard]] double to_epoch(TTimePoint timeIn) {
    return std::chrono::duration<double>(timeIn.time_since_epoch()).count();
}

} // namespace sg::time