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
    /* re-entry from inside the stopped-callback would deadlock (run() waits for
     * the cycle to finish, which is finalised by the callback thread itself). */
    if (m_cb_thread_id.load(std::memory_order_acquire) == std::this_thread::get_id())
        SG_THROW(std::logic_error,
                 "asio_io_pool::run() must not be called from the stopped callback");

    /* can't interleave with stop_async() or another run() */
    std::lock_guard lock(m_mutex);

    /* wait out any in-flight cycle. A cycle whose context is still live is
     * healthy — nothing to do. A stopped context means the cycle is winding
     * down (stop_async(), or a guardless pool that ran out of work — possibly
     * so recently that the workers are still inside m_context.run()); handlers
     * posted now would be stranded, so wait for the monitor thread to publish
     * completion and start a fresh cycle. */
    while (m_cycle_active.load(std::memory_order_acquire)) {
        if (!m_context.stopped())
            return false;
        m_cycle_active.wait(true, std::memory_order_acquire);
    }

    m_context.restart();
    if (m_guard_enabled)
        m_guard.emplace(boost::asio::make_work_guard(m_context));

    /* must be set before the monitor thread exists: on an instant drain the
     * monitor's completing store(false) would otherwise be overwritten by this
     * store(true), wedging the pool. */
    m_cycle_active.store(true, std::memory_order_release);

    try {
        m_workers.reserve(m_no_workers);
        for (size_t i = 0; i < m_no_workers; ++i)
            m_workers.emplace_back([this]() { m_context.run(); });

        /* monitor: joining the workers blocks until the cycle ends (stop or
         * natural drain), so no signalling from the workers is needed. Then run
         * the user callback and publish completion. The previous cycle's
         * m_cb_thread (if any) is joined by this move-assignment; it has
         * already published completion. */
        m_cb_thread = std::jthread([this]() {
            m_workers.clear();  // joins all workers

            if (m_on_stopped_call_back) {
                m_cb_thread_id.store(std::this_thread::get_id(),
                                     std::memory_order_release);
                m_on_stopped_call_back.invoke(*this);
                m_cb_thread_id.store(std::thread::id{},
                                     std::memory_order_release);
            }

            m_cycle_active.store(false, std::memory_order_release);
            m_cycle_active.notify_all();
        });

        return true;
    } catch (...) {
        m_guard.reset();
        if (!m_context.stopped())
            m_context.stop();
        m_workers.clear();  // joins any workers that did start

        m_cycle_active.store(false, std::memory_order_release);
        m_cycle_active.notify_all();
        throw;
    }
}

boost::asio::io_context& asio_io_pool::context() { return m_context; }

const boost::asio::io_context& asio_io_pool::context() const {
    return m_context;
}

bool asio_io_pool::is_running() const {
    return m_cycle_active.load(std::memory_order_acquire) && !m_context.stopped();
}

size_t asio_io_pool::worker_count() const { return m_no_workers; }

asio_io_pool::state_t asio_io_pool::state() const {
    if (!m_cycle_active.load(std::memory_order_acquire))
        return state_t::stopped;
    return m_context.stopped() ? state_t::stopping : state_t::running;
}

void asio_io_pool::stop_async() {
    /* needs mutex so we don't interleave with run() */
    std::lock_guard lock(m_mutex);

    /* no-op if no cycle is in flight. A cycle already winding down falls
     * through harmlessly: resetting an empty guard and stopping a stopped
     * context both do nothing. */
    if (!m_cycle_active.load(std::memory_order_acquire))
        return;

    m_guard.reset();

    if (!m_context.stopped())
        m_context.stop();
}

void asio_io_pool::wait_for_stop() const {
    /* re-entry from inside the stopped-callback would wait for our own thread to
     * publish completion — return instead; the stop completes once the callback
     * returns. */
    if (m_cb_thread_id.load(std::memory_order_acquire) == std::this_thread::get_id())
        return;

    while (m_cycle_active.load(std::memory_order_acquire))
        m_cycle_active.wait(true, std::memory_order_acquire);
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
