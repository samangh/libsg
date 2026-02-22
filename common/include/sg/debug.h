#pragma once

#include <fmt/format.h>
#include <string_view>
#include <source_location>

#ifdef LISBG_STACKTRACE
    #include <boost/stacktrace.hpp>
#endif

namespace sg {

#if defined(LISBG_STACKTRACE)
    /**
     * Throws an exception, with a stacktrace.
     */
    #define SG_THROW_STACKTRACE(type, what, ...)                                                   \
        do {                                                                                       \
            auto location = std::source_location::current();                                       \
            throw type(fmt::format("{}\nat `{}` in {}({}:{}), with stacktrace:\n{}", what,         \
                                   location.function_name(), location.file_name(),                 \
                                   location.line(), location.column(),                             \
                                   to_string(boost::stacktrace::stacktrace()))                     \
                                   __VA_OPT__(,) __VA_ARGS__);                                     \
        } while (0)
#endif

/**
 * Throws an exception, including the function name, file path and line number/column in the
 * message.
 */
#define SG_THROW_DETAILS(type, what, ...)                                                          \
    do {                                                                                           \
        auto location = std::source_location::current();                                           \
        throw type(fmt::format("{}, at `{}` in {}({}:{})", what, location.function_name(),         \
                               location.file_name(), location.line(), location.column())           \
                                __VA_OPT__(,) __VA_ARGS__);                                        \
    } while (0)

#if defined(LISBG_STACKTRACE)
    #define SG_THROW(type, what, ...) SG_THROW_STACKTRACE(type, what __VA_OPT__(,) __VA_ARGS__)
#elif defined(LISBG_EXCEPTION_DETAILS)
    #define SG_THROW(type, what, ...) SG_THROW_DETAILS(type, what __VA_OPT__(,) __VA_ARGS__)
#else
    #define SG_THROW(type, what, ...) throw type(what __VA_OPT__(,) __VA_ARGS__)
#endif

#if defined(LISBG_STACKTRACE) || defined(LISBG_EXCEPTION_DETAILS)
    #define SG_CATCH_RETHROW(func)                                                                 \
        do {                                                                                       \
            try {                                                                                  \
                (func);                                                                            \
            } catch (const std::exception& ex) { SG_THROW(std::runtime_error, ex.what()); }        \
        } while (0)
#else
    #define SG_CATCH_RETHROW(func)                                                                 \
        do { (func); } while (0)
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
