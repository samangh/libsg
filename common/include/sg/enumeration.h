#pragma once

#include "sg/debug.h"

#include <fmt/format.h>
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
    requires std::is_enum_v<enumT>
[[nodiscard]] bool contains(enumT enumeration, enumT enum_to_check) {
    using T = std::underlying_type_t<enumT>;
    return (static_cast<T>(enumeration) & static_cast<T>(enum_to_check)) ==
           static_cast<T>(enum_to_check);
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


template<typename T>
    requires(std::is_enum_v<T>)
class helper {
  public:
    helper(){};
    helper(std::initializer_list<std::pair<T, std::string>> list) {
        for (const auto& [val, name] : list)
            add(val, name);
    }

    virtual ~helper() =default;

    void add(T enumVal, std::string name ){
        if (m_name_map.contains(name))
            throw std::logic_error(fmt::format("duplicate enum value name {} specific", name));

        if (m_value_map.contains(enumVal))
            throw std::logic_error(fmt::format("duplicate enum value {}, with name {} specificed", sg::enumeration::underlying_value(enumVal), name));

        m_name_map.emplace(name,enumVal);
        m_value_map.emplace(enumVal, name);
    }

    [[nodiscard]] std::string to_string(T value) const{
        return m_value_map.at(value);
    }

    [[nodiscard]] T to_enum(std::string name) {
        return m_name_map.at(name);
    }

  private:
    std::map<std::string, T> m_name_map;
    std::map<T, std::string> m_value_map;
};

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
    static auto map = []() {
        std::map<TEnum, TVal> map_;
        populate_enum_value_map(map_);
        return map_;
    }();

    return map.at(en);
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
    static auto map_ = []() {
        /* get map */
        std::map<TEnum, TValue> map_;
        populate_enum_value_map(map_);

        /* reverse map */
        std::map<TValue, TEnum> map_rev;

        /* ensure there are no duplicates */
        for (const auto& [e, v] : map_) {
            if (map_rev.contains(v))
                throw std::logic_error(fmt::format("duplicate enum name/value specified for {}",
                                                   sg::type_name<TEnum>()));
            map_rev.emplace(v, e);
        }

        return map_rev;
    }();

    return map_.at(val);
}

} // namespace sg::enumeration
