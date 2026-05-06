#include "sg/asio_io_pool.h"

#include "sg/debug.h"

#include <boost/asio.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <utility>

namespace sg::net {

asio_io_pool::asio_io_pool(Private, size_t noWorkers, bool enableGuard,
                           stopped_cb_t onStoppedCallBack)
    : m_context(noWorkers),
      m_no_workers(noWorkers),
      m_guard_enabled(enableGuard),
      m_on_stopped_call_back(std::move(onStoppedCallBack)) {
    // this can never be zero, if as create() will set this to hardware_concurrency()  or 1 if zero
    // is passed
    assert (noWorkers != 0);
}

asio_io_pool::~asio_io_pool() {
    stop_async();
    wait_for_stop();
}

std::shared_ptr<asio_io_pool> asio_io_pool::create(size_t noWorkers, bool enableGuard,
                                                   stopped_cb_t onStoppedCallBack) {
    if (noWorkers == 0)
        noWorkers = std::thread::hardware_concurrency();

    // hardware_concurrency() can return zero if the concurrency can't be calcualted, in which case
    // we'll just set this to 1
    if (noWorkers == 0)
        noWorkers = 1;
    return std::make_shared<asio_io_pool>(Private(), noWorkers, enableGuard,
                                          std::move(onStoppedCallBack));
}

bool asio_io_pool::run() {
    /* re-entry from inside the stopped-callback would deadlock on the mutex (run()
     * waits for state_t::stopped, which is set by the callback thread itself). */
    if (m_cb_thread_id.load(std::memory_order_acquire) == std::this_thread::get_id())
        SG_THROW(std::logic_error,
                 "asio_io_pool::run() must not be called from the stopped callback");

    /* can't interleave with stop_async() or another run() */
    std::lock_guard lock(m_mutex);

    /* claim the running state, waiting out any in-flight previous cycle. */
    auto valState = state_t::stopped;
    while (!m_state.compare_exchange_weak(valState, state_t::running)) {
        if (valState == state_t::running)
            return false;
        // valState is stopping; wait for the previous cycle's m_cb_thread to publish stopped.
        m_state.wait(valState, std::memory_order_acquire);
        valState = state_t::stopped;
    }

    m_context.restart();
    if (m_guard_enabled)
        m_guard.emplace(boost::asio::make_work_guard(m_context));

    try {
        m_workers.reserve(m_no_workers);
        for (size_t i = 0; i < m_no_workers; ++i) {
            m_workers.emplace_back([this]() {
                m_context.run();

                /* signal natural drain so the monitor thread wakes. */
                auto s = state_t::running;
                if (m_state.compare_exchange_strong(s, state_t::stopping))
                    m_state.notify_all();
            });
        }

        /* monitor: waits for the cycle to leave running, joins workers, runs the user
         * callback, then publishes stopped. The previous cycle's m_cb_thread (if any)
         * is joined by this move-assignment; it has already published stopped above. */
        m_cb_thread = std::jthread([this]() {
            while (m_state.load(std::memory_order_acquire) == state_t::running)
                m_state.wait(state_t::running, std::memory_order_acquire);

            m_workers.clear();  // joins all workers

            if (m_on_stopped_call_back) {
                m_cb_thread_id.store(std::this_thread::get_id(),
                                     std::memory_order_release);
                m_on_stopped_call_back.invoke(*this);
                m_cb_thread_id.store(std::thread::id{},
                                     std::memory_order_release);
            }

            m_state.store(state_t::stopped, std::memory_order_release);
            m_state.notify_all();
        });

        return true;
    } catch (...) {
        if (m_guard)
            m_guard.reset();
        if (!m_context.stopped())
            m_context.stop();
        m_workers.clear();  // joins any workers that did start

        m_state.store(state_t::stopped);
        m_state.notify_all();
        throw;
    }
}

boost::asio::io_context& asio_io_pool::context() { return m_context; }

const boost::asio::io_context& asio_io_pool::context() const {
    return m_context;
}

bool asio_io_pool::is_running() const {
    return m_state.load(std::memory_order_acquire) == state_t::running;
}

size_t asio_io_pool::worker_count() const { return m_no_workers; }

asio_io_pool::state_t asio_io_pool::state() const {
    return m_state.load(std::memory_order_acquire);
}

void asio_io_pool::stop_async() {
    /* needs mutex so we don't interleave with run() */
    std::lock_guard lock(m_mutex);

    /* no-op if already stopping or stopped */
    auto valState = state_t::running;
    if (!m_state.compare_exchange_strong(valState, state_t::stopping))
        return;

    /* wake the monitor thread waiting on state != running. */
    m_state.notify_all();

    if (m_guard)
        m_guard.reset();

    if (!m_context.stopped())
        m_context.stop();
}

void asio_io_pool::wait_for_stop() {
    /* re-entry from inside the stopped-callback would wait for our own thread to set
     * state_t::stopped — return instead; the stop completes once the callback returns. */
    if (m_cb_thread_id.load(std::memory_order_acquire) == std::this_thread::get_id())
        return;

    while (true) {
        const auto val = m_state.load(std::memory_order_acquire);
        if (val == state_t::stopped)
            return;

        m_state.wait(val, std::memory_order_acquire);
    }
}

bool asio_io_pool::has_guard() const {
    return m_guard_enabled;
}

void asio_io_pool::restart() {
    stop_async();
    wait_for_stop();
    run();
}

} // namespace sg::net
