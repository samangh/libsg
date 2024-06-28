#pragma once

#include <sg/enumeration.h>

#include <type_traits>

namespace sg::imgui {

enum class ConfigFlags {
    None,
    IncludeImPlot,
    Docking,
    ViewPort,
    NoIni // Disable imgui.ini file
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
