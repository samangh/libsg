#pragma once

#include <concepts>
#include <optional>
#include <chrono>

namespace sg::concepts {

template <typename T>
concept is_optional = std::is_same_v<T, std::optional<typename T::value_type>>;

template <typename T>
concept is_time_point_v =
    std::is_same_v<T, std::chrono::time_point<typename T::clock, typename T::duration>>;

} // namespace sg::concepts
