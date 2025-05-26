#pragma once

#include <fmt/compile.h>
#include <fmt/format.h>

#include <cstdint>
#include <string>
#include <type_traits>

namespace sg::math {

/* Returns the largest EXACT factor of 2 that the input data size is divisible by
 *
 * For example:
 *  - input '8' returns 8
 *  - input '9' returns 1
 */
template <typename T>
    requires(std::is_unsigned_v<T>)
[[nodiscard]] T exact_factor_of_two(T number) {
    return number & (~number + 1);
}

/* Returns the largest factor of 2 that the input data size is divisible by
 *
 * For example:
 *  - input '9' returns 8
 *  - input '16' returns 16
 */
template <typename T>
    requires(std::is_unsigned_v<T>)
[[nodiscard]] T largest_power_of_two(T number) {
    for (size_t i=1; i<=sizeof(T)*8/2; i*=2)
        number = number | (number >> i);
    return number - (number >> 1);

    /* Effectively does (e.g. for uint32_t:
     *    number = number | (number >> 1);
     *    number = number | (number >> 2);
     *    number = number | (number >> 4);
     *    number = number | (number >> 8);
     *    number = number | (number >> 16);
     */
}

} // namespace sg::math
