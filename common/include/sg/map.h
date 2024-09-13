#pragma once

#include <map>

namespace sg::map {

template <typename T, typename K>
[[nodiscard]] [[deprecated("use contains() function")]] inline static bool has_key(
    const std::map<T, K>& map, const T& key) {
    return map.contains(key);
}

} // namespace sg::map
