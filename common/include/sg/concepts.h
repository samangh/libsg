#pragma once

#include <concepts>
#include <optional>

namespace sg::concepts {

template <typename T>
concept is_optional = std::is_same_v<T, std::optional<typename T::value_type>>;

} // namespace sg::concepts
