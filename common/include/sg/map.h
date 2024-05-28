#pragma once

#include <map>

namespace sg::map {

template <typename T, typename K>
inline static bool has_key(const std::map<T, K> &map, const T &key) {
    return (map.find(key) != map.end());
}

} // namespace sg::map
