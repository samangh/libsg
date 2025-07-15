#pragma once

#include <sg/enumeration.h>

#include <type_traits>

namespace sg::imgui {

enum class ConfigFlags : uint32_t {
    None          = 1 << 0,
    IncludeImPlot = 1 << 1,
    Docking       = 1 << 2,
    ViewPort      = 1 << 3,
    NoIni         = 1 << 4
};

inline constexpr ConfigFlags operator|(ConfigFlags lhs, ConfigFlags rhs) {
    return sg::enumeration::operator|(lhs, rhs);
}

inline constexpr ConfigFlags& operator|=(ConfigFlags& lhs, ConfigFlags rhs) {
    return sg::enumeration::operator|=(lhs, rhs);
}

inline constexpr ConfigFlags operator&(ConfigFlags lhs, ConfigFlags rhs) {
    return sg::enumeration::operator&(lhs, rhs);
}

} // namespace sg::imgui
