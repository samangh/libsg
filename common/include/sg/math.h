#pragma once

#include <fmt/compile.h>
#include <fmt/format.h>

#include <cstdint>
#include <string>
#include <type_traits>

namespace sg::math {

/* Returns the largest EXACT factor of 2 that the input data size is divisble by
 *
 * For exmaple:
 *  - input '8' returns 8
 *  - input '9' returns 1
*/
template <typename T, typename = typename std::enable_if<std::is_unsigned<T>::value>::type>
[[nodiscard]] T exact_factor_of_two(T number)
{
    return number & (~number + 1);
}

/* Returns the largest factor of 2 that the input data size is divisble by
 *
 * For exmaple:
 *  - input '9' returns 8
 *  - input '16' returns 16
*/
template <typename T, typename = typename std::enable_if<std::is_unsigned<T>::value>::type>
[[nodiscard]] T largest_power_of_two(T number)
{
    number = number| (number>>1);
    number = number| (number>>2);
    number = number| (number>>4);
    number = number| (number>>8);
    number = number| (number>>16);
  return number - (number>>1);
}

/* Fast number to string conversion */
template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
[[nodiscard]] inline std::string to_string(T number) {
    return (fmt::to_string(number));
}

}
