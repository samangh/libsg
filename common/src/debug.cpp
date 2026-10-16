#include <sg/debug.h>

#include <fmt/format.h>

#include <atomic>
#include <iostream>
#include <mutex>
#include <ostream>
#include <string>

#ifdef LIBSG_STACKTRACE
    #include <boost/stacktrace.hpp>
#endif

namespace {

std::atomic g_mode{sg::debug::exception_trace_mode::none};

std::mutex g_mutex;
/* default is std::cerr wit the deleter is removed for std:cerr.*/
std::shared_ptr<std::ostream> g_stream{std::shared_ptr<void>{}, &std::cerr};

} // namespace

namespace sg::debug {

void set_trace_mode(exception_trace_mode mode) noexcept {
#ifndef LIBSG_STACKTRACE
    if (mode > exception_trace_mode::details)
        mode = exception_trace_mode::details;
#endif
    g_mode.store(mode, std::memory_order_relaxed);
}

exception_trace_mode trace_mode() noexcept { return g_mode.load(std::memory_order_relaxed); }

void set_trace_stream(std::shared_ptr<std::ostream> stream) {
    const std::lock_guard lock(g_mutex);
    g_stream = std::move(stream);
}

std::shared_ptr<std::ostream> trace_stream() {
    const std::lock_guard lock(g_mutex);
    return g_stream;
}

void detail::trace_throw(const std::source_location& location) noexcept {
    const auto mode = g_mode.load(std::memory_order_relaxed);
    if (mode == exception_trace_mode::none)
        return;

    /* we are about to throw, so a failure here must not replace that exception */
    try {
        const std::lock_guard lock(g_mutex);
        if (!g_stream)
            return;

        auto message = fmt::format("throw at `{}` in {}({}:{})", location.function_name(),
                                   location.file_name(), location.line(), location.column());
#ifdef LIBSG_STACKTRACE
        if (mode == exception_trace_mode::stacktrace)
            message +=
                fmt::format(", with stacktrace:\n{}", to_string(boost::stacktrace::stacktrace()));
#endif
        *g_stream << message << std::endl;
    } catch (...) {}
}

} // namespace sg::debug
