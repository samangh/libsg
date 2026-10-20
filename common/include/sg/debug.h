#pragma once

#include <sg/export/common.h>

#include <fmt/format.h>
#include <iosfwd>
#include <memory>
#include <source_location>
#include <string_view>
#include <utility>

namespace sg::debug {

/** Ordered (so that @c none < @c details).
 *
 * Note that stacktrace needs LIBSG_STACKTRACE, and is clamped to @c details without it. */
enum class exception_trace_mode { none, details, stacktrace };

SG_COMMON_EXPORT void set_trace_mode(exception_trace_mode mode) noexcept;
[[nodiscard]] SG_COMMON_EXPORT exception_trace_mode trace_mode() noexcept;

/* Takes shared ownership of the stream. Defaults to std::cerr; nullptr discards. */
SG_COMMON_EXPORT void set_trace_stream(std::shared_ptr<std::ostream> stream);
[[nodiscard]] SG_COMMON_EXPORT std::shared_ptr<std::ostream> trace_stream();

/* Writes to trace log (followed std::endl), to the trace stream. Thread-safe. */
SG_COMMON_EXPORT void write_trace(std::string_view msg) noexcept;
template <typename... Args>
void write_trace(fmt::format_string<Args...> fmt, Args&&... args) noexcept {
    try {
        write_trace(std::string_view(fmt::format(fmt, std::forward<Args>(args)...)));
    } catch (...) {}
}

namespace internal {
    SG_COMMON_EXPORT void trace_throw(const std::source_location& location) noexcept;
} // namespace internal

} // namespace sg::debug

/**
 * Throws an exception, tracing it first; see sg::debug::set_trace_mode().
 *
 * The exception is left alone, so any constructor can be used.
 */
#define SG_THROW(type, ...)                                                                        \
    do {                                                                                           \
        sg::debug::internal::trace_throw(std::source_location::current());                         \
        throw type(__VA_ARGS__);                                                                   \
    } while (0)

namespace sg {

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
