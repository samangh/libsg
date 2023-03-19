#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

namespace sg::bounds {

/**
 * @brief returns the index of the first element with a value higher than the specified value.
 */
template <typename T, typename Value>
size_t upper_bound_index(const T* data, size_t count, const Value &to_find) {
    auto iterator = std::upper_bound(data, data+count, to_find);
    if (iterator == data+count)
        return count - 1;
    else
        return iterator-data; //faster than std::distance(data, upper);
}

/**
 * @brief returns the index of first element that is equal or greater than the specified value.
 */
template <typename T, typename Value>
size_t greater_or_equal_index(const T* data, size_t count, const Value &to_find) {
    auto i = upper_bound_index(data, count, to_find);

    if (i !=0 && data[i] >= to_find)
        return --i;
    return i;
}

/**
 * @brief returns the index of the first element with a value lower than the specified value.
 */
template <typename T, typename Value>
size_t lower_bound_index(const T* data, size_t count, const Value &to_find) {
    auto iterator = std::lower_bound(data, data+count, to_find);
    if (iterator == data+count)
        return count - 1;
    else
        return iterator-data; //faster than std::distance(data, lower)
}

template <typename T, typename Value>
size_t less_or_equal_index(const T* data, size_t count, const Value &to_find) {
    auto i = lower_bound_index(data, count, to_find);

    if (i != count-1 && data[i+1] <= to_find)
        return ++i;
    return i;
}

template <template <typename, typename> class Container,
          typename Value,
          typename Allocator = std::allocator<Value>>
size_t upper_bound_index(const Container<Value, Allocator> &data, const Value &to_find) {
    auto upper = std::upper_bound(data.begin(), data.end(), to_find);
    if (upper == data.end())
        return data.size() - 1;
    else
        return std::distance(data.begin(), upper);
}

template <template <typename, typename> class Container,
          typename Value,
          typename Allocator = std::allocator<Value>>
size_t lower_bound_index(const Container<Value, Allocator> &data, const Value &to_find) {
    auto upper = std::lower_bound(data.begin(), data.end(), to_find);
    if (upper == data.end())
        return data.size() - 1;
    else
        return std::distance(data.begin(), upper);
}

} // namespace sg::upper
