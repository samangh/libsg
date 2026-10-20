#pragma once

#include <sg/export/common.h>

/* Only the core formatting API is needed here, not fmt::format() itself; keeping <fmt/format.h>
 * out matters because every header wanting SG_THROW includes this one. fmt renamed that core
 * header from "core.h" to "base.h" in v11, and v11+ ships a "core.h" that just pulls in all of
 * <fmt/format.h>, so the spelling has to follow the version. */
#if __has_include(<fmt/base.h>)
    #include <fmt/base.h>
#else
    #include <fmt/core.h>
#endif

#include <iosfwd>
#include <memory>
#include <source_location>
#include <string_view>

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

namespace internal {
    SG_COMMON_EXPORT void trace_throw(const std::source_location& location) noexcept;

    /* Formats and writes, taking the arguments type-erased so that the overload below needs only
     * <fmt/base.h>; pulling in <fmt/format.h> here would cost every header that wants SG_THROW. */
    SG_COMMON_EXPORT void vwrite_trace(fmt::string_view fmt_str, fmt::format_args args) noexcept;
} // namespace internal

/* Note, the format string is checked at compile time; use fmt::runtime() for one built at run
 * time. */
template <typename... Args>
void write_trace(fmt::format_string<Args...> fmt_str, Args&&... args) noexcept {
    internal::vwrite_trace(fmt_str, fmt::make_format_args(args...));
}

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
