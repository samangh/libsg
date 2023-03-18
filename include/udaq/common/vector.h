#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

namespace sg::vector {

template <typename T>
void append(std::vector<T> &base, const std::vector<T> to_add) {
    base.insert(std::end(base), std::begin(to_add), std::end(to_add));
}

} // namespace sg::vector
