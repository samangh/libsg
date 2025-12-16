#pragma once

#include "internal/enumeration.h"

#include <map>
#include <string>
#include <type_traits>
#include <utility>

namespace sg::enumeration {

template <typename enumT>
    requires std::is_enum_v<enumT>
[[nodiscard]] constexpr enumT operator|(enumT lhs, enumT rhs) {
    using T = std::underlying_type_t<enumT>;
    return static_cast<enumT>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

template <typename enumT>
    requires std::is_enum_v<enumT>
constexpr enumT& operator|=(enumT& lhs, enumT rhs) {
    lhs = lhs | rhs;
    return lhs;
}

template <typename enumT>
    requires std::is_enum_v<enumT>
[[nodiscard]] constexpr enumT operator&(enumT lhs, enumT rhs) {
    using T = std::underlying_type_t<enumT>;
    return static_cast<enumT>(static_cast<T>(lhs) & static_cast<T>(rhs));
}

template <typename enumT>
[[nodiscard]] bool contains(enumT enumeration, enumT enum_to_check) {
    return ((enumeration & enum_to_check) == enum_to_check);
}

template <typename enumT, typename T = std::underlying_type_t<enumT>>
    requires std::is_enum_v<enumT>
[[nodiscard]] constexpr T underlying_value(enumT enumeration) {
    return static_cast<T>(enumeration);
}

template <typename enumT, typename T = std::underlying_type_t<enumT>>
    requires std::is_enum_v<enumT>
[[nodiscard]] constexpr enumT from_underlying_value(T underlying_value) {
    return static_cast<enumT>(underlying_value);
}

/************************************* enum/value conversion *************************************/

/**
 * @brief Serialises an enum to a different type, e.g. enum to a string representation
 *
 *        Uses Argument Dependent Lookup (AD), and assumes that a function with signature
 *        populate_enum_value_map(std::map<TEnum,TVal>&) exists
 */
template<typename TVal,typename TEnum>
    requires(std::is_enum_v<TEnum>)
[[nodiscard]] TVal enum_to_val(TEnum en) {
    static auto bimap = sg::internal::enumeration::generate_enum_val_bimap<TEnum,TVal>();
    return bimap.left.at(en);
}

/**
 * @brief Deserialises an enum from a different type, e.g. a string to enum.
 *
 *        Uses Argument Dependent Lookup (AD), and assumes that a function with signature
 *        populate_enum_value_map(std::map<TEnum,TVal>&) exists
 */
template <typename TEnum, typename TValue>
    requires(std::is_enum_v<TEnum>)
[[nodiscard]] TEnum enum_from_val(TValue val) {
    static auto bimap = sg::internal::enumeration::generate_enum_val_bimap<TEnum,TValue>();
    return bimap.right.at(val);
}

} // namespace sg::enumeration
