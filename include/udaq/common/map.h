#pragma once

#include "udaq/export/common.h"

#include <map>
#include <string>

namespace udaq::common::map {

template <typename T, typename K>
inline static bool has_key(const std::map<T, K>& map, const T& key) {
    return (map.find(key) != map.end());
}

}
