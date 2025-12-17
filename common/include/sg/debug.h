#pragma once

#include <fmt/format.h>
#include <string_view>
#include <source_location>

#ifdef LISBG_STACKTRACE
    #include <boost/stacktrace.hpp>
#endif

namespace sg {

/* LISBG_STACKTRACE is set by the LISBG_STACKTRACE Cmake project option */
#ifdef LISBG_STACKTRACE
    #define SG_THROW_EXCEPTION(type, what)                                                         \
        do {                                                                                       \
            auto location = std::source_location::current();                                       \
            throw type(fmt::format("{}\nat {} in {}({}:{}), with stacktrace:\n{}", what,           \
                                   location.function_name(), location.file_name(),                 \
                                   location.line(), location.column(),                             \
                                   to_string(boost::stacktrace::stacktrace())));                   \
        } while (0)
#else
do {
    auto location = std::source_location::current();
    throw type(fmt::format("{}\nat {} in {}({}:{})", what,
                           location.function_name(), location.file_name(), location.line(),
                           location.column())));
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
