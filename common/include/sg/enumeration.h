#pragma once

#include <type_traits>

namespace sg::enumeration {

template<typename enumT, typename std::enable_if<std::is_enum<enumT>::value>::type* = nullptr>
inline constexpr enumT operator | (enumT lhs, enumT rhs)
{
    using T = std::underlying_type_t<enumT>;
    return static_cast<enumT>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

template<typename enumT, typename std::enable_if<std::is_enum<enumT>::value>::type* = nullptr>
inline constexpr  enumT& operator |= (enumT& lhs, enumT rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

template<typename enumT, typename std::enable_if<std::is_enum<enumT>::value>::type* = nullptr>
inline constexpr enumT operator&(enumT lhs, enumT rhs)
{
    using T = std::underlying_type_t<enumT>;
    return static_cast<enumT> (static_cast<T>(lhs) & static_cast<T>(rhs));
}


template<typename enumT, typename std::enable_if<std::is_enum<enumT>::value>::type* = nullptr>
inline bool contains(enumT enumeration, enumT enum_to_check)
{
    using T = std::underlying_type_t<enumT>;
    return (static_cast<T>(enumeration) & static_cast<T>(enum_to_check)) == static_cast<T>(enum_to_check);
}

}
