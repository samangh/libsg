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
