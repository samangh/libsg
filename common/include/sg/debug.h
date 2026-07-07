#pragma once

#include <fmt/format.h>
#include <string>
#include <string_view>
#include <source_location>

#ifdef LIBSG_STACKTRACE
    #include <boost/stacktrace.hpp>
#endif

namespace sg {

/* note: the SG_THROW_DETAILS macro assumes that you can construct an exception
 * with only a single argument that represents teh exception message */

#ifdef LIBSG_STACKTRACE
/**
 * Returns the (symbolised) stacktrace captured at the original throw site.
 *
 * When LIBSG_STACKTRACE is enabled, the boost_stacktrace_from_exception library captures the
 * raw stack frames of every thrown exception. Capture is cheap - the expensive symbolisation only
 * happens here on demand. This keeps throwing cheap on hot/error paths.
 */
inline std::string current_exception_stacktrace() {
    if (const auto trace = boost::stacktrace::stacktrace::from_current_exception(); trace)
        return to_string(trace);
    return {};
}
#endif

/**
 * Throws an exception, including the function name, file path and line number/column in the
 * message.
 */
#define SG_THROW_DETAILS(type, ...)                                                                \
    do {                                                                                           \
        const type ex{__VA_ARGS__};                                                                \
        auto location = std::source_location::current();                                           \
        throw type(fmt::format("{}, at `{}` in {}({}:{})", ex.what(), location.function_name(),    \
                               location.file_name(), location.line(), location.column()));         \
    } while (0);

/* With LIBSG_STACKTRACE the stacktrace is captured automatically at the throw point (see
 * sg::current_exception_stacktrace() above), so SG_THROW only needs to add the source
 * location to the message. */
#if defined(LIBSG_STACKTRACE) || defined(LIBSG_EXCEPTION_DETAILS)
    #define SG_THROW(type, ...) SG_THROW_DETAILS(type __VA_OPT__(,) __VA_ARGS__)
#else
    #define SG_THROW(type, ...) throw type(__VA_ARGS__)
#endif

#if defined(LIBSG_STACKTRACE) || defined(LIBSG_EXCEPTION_DETAILS)
    #define SG_CATCH_RETHROW(func)                                                                 \
        do {                                                                                       \
            try {                                                                                  \
                (func);                                                                            \
            } catch (const std::exception& exc) { SG_THROW(std::runtime_error, exc.what()); }      \
        } while (0);
#else
    #define SG_CATCH_RETHROW(func)                                                                 \
        do { (func); } while (0);
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
