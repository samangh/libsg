#pragma once

#include <vector>
#include <cstdint>
#include <type_traits>
#include <memory>
#include <algorithm>

namespace udaq::common::vector {

template<typename T>
void append(std::vector<T>& base, const std::vector<T> to_add)
{
    base.insert(std::end(base), std::begin(to_add), std::end(to_add));
}

template <template <typename, typename> class Container,
    typename Value,
    typename Allocator = std::allocator<Value>>
size_t upper_bound_index(const Container<Value, Allocator>& data, const Value& to_find)
{
    auto upper = std::upper_bound(data.begin(), data.end(), to_find);
    if (upper == data.end())
        return data.size() - 1;
    else
        return  std::distance(data.begin(), upper);
}

template <template <typename, typename> class Container,
    typename Value,
    typename Allocator = std::allocator<Value>>
size_t lower_bound_index(const Container<Value, Allocator>& data, const Value& to_find)
{
    auto upper = std::lower_bound(data.begin(), data.end(), to_find);
    if (upper == data.end())
        return data.size() - 1;
    else
        return std::distance(data.begin(), upper);
}

}
