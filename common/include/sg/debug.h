#pragma once

#include <fmt/format.h>
#include <string_view>

namespace sg {

#if defined(_MSC_VER)
    #define THROW_DEBUG_EXCEPTION(type, what)                                                      \
        do {                                                                                       \
            throw type(fmt::format("{} (in {}, {}:{})", what, __FUNCSIG__, __FILE__, __LINE__));   \
        } while (0)
#else
    #define THROW_DEBUG_EXCEPTION(type, what)                                                      \
        do {                                                                                       \
            throw type(                                                                            \
                fmt::format("{} (in {}, {}:{})", what, __PRETTY_FUNCTION__, __FILE__, __LINE__));  \
        } while (0)
#endif

/// Returns the type of the passed object as a string.
///
/// Use decltype() on the input variable. As an example,
/// `type_name(decltype<i>)`will retrun 'int' if 'i' is an int.
template <typename T> constexpr auto type_name() noexcept {
    // This is from
    // https://stackoverflow.com/questions/81870/is-it-possible-to-print-a-variables-type-in-standard-c/56766138#56766138

    // This function must be in the header

    std::string_view name = "Error: unsupported compiler", prefix, suffix;
#ifdef __clang__
    name = __PRETTY_FUNCTION__;
    prefix = "auto sg::type_name() [T = ";
    suffix = "]";
#elif defined(__GNUC__)
    name = __PRETTY_FUNCTION__;
    prefix = "constexpr auto sg::type_name() [with T = ";
    suffix = "]";
#elif defined(_MSC_VER)
    name = __FUNCSIG__;
    prefix = "auto __cdecl sg::type_name<";
    suffix = ">(void) noexcept";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

} // namespace sg
