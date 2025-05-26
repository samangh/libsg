#pragma once

#include <algorithm>
#include <random>
#include <vector>
#include <numeric>

namespace sg::random {

/* generate a vector of data (in range 0 to count, shuffled)*/
template<std::integral T>
std::vector<T> generate(std::size_t count) {
    std::vector<T> data(count);
    std::iota(data.begin(), data.end(), static_cast<T>(0));
    std::shuffle(data.begin(), data.end(), std::mt19937{std::random_device{}()});
    return data;
}

} // namespace sg
