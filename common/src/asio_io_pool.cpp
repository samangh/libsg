#include "sg/asio_io_pool.h"

#include "sg/debug.h"

#include <boost/asio.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <latch>
#include <utility>

namespace sg::net {

asio_io_pool::asio_io_pool(Private, size_t noWorkers, bool enableGuard,
                           stopped_cb_t onStoppedCallBack)
    : m_context(noWorkers),
      m_no_workers(noWorkers),
      m_guard_enabled(enableGuard),
      m_on_stopped_call_back(std::move(onStoppedCallBack)) {
    if (noWorkers == 0)
        SG_THROW(std::invalid_argument, "noWorkers must be > 0");
}

asio_io_pool::~asio_io_pool() {
    stop_async();
    wait_for_stop();
}

std::shared_ptr<asio_io_pool> asio_io_pool::create(size_t noWorkers, bool enableGuard,
                                                   stopped_cb_t onStoppedCallBack) {
    if (noWorkers == 0)
        noWorkers = std::thread::hardware_concurrency();
    return std::make_shared<asio_io_pool>(Private(), noWorkers, enableGuard,
                                          std::move(onStoppedCallBack));
}

bool asio_io_pool::run() {
    /* can't interleave with stop_async() or another run() */
    std::lock_guard lock(m_mutex);

    /* latch needs to be shared_ptr. This is because a std::latch .arrive_and_wait() is actually
     * ".count_down(); .wait();" so there is a chance that a thread might be in-between
     * these two calls, whilst the .wait() on the main thread succeeds and destructs the latch */
    auto latch_ = std::make_shared<std::latch>(static_cast<ptrdiff_t>(m_no_workers));
    std::atomic<size_t> posts_started(0);

    try {
        auto valState = state_t::stopped;
        while (!m_state.compare_exchange_weak(valState, state_t::starting)) {
            if (valState == state_t::running)
                return false;
            // wait until stopped!
            if (valState == state_t::stopping)
                m_state.wait(valState, std::memory_order_acquire);
        }

        m_context.restart();
        if (m_guard_enabled)
            m_guard.emplace(boost::asio::make_work_guard(m_context));

        /* start the pool */
        m_pool = std::make_unique<boost::asio::thread_pool>(m_no_workers);

        /* just set number of running workers to m_no_workers, as there is a latch that ensures it */
        m_running_worker_threads_count.store(m_no_workers);

        for (size_t i = 0; i < m_no_workers; ++i) {
            boost::asio::post(m_pool->executor(), [this, latch_]() {
                /* need to have all pools running, just in case a callback in m_context runs
                 * async_stop() */
                latch_->arrive_and_wait();

                m_context.run();

                auto state_ = state_t::running;
                m_state.compare_exchange_strong(state_, state_t::stopping);

                if (--m_running_worker_threads_count == 0) {
                    /* run callback is separate loop to prevent any self-join locks in the callback
                     *
                     * note: internally joins all threads, which provides a happens-before edge with
                     * everything that happened inside the pool tasks, so no need to add any locks in
                     * here  */
                    m_cb_thread = std::jthread([this]() {
                        if (m_pool)
                            m_pool->join();

                        if (m_on_stopped_call_back)
                            m_on_stopped_call_back.invoke(*this);

                        m_state.store(state_t::stopped);
                        m_state.notify_all();
                    });
                }
            });
            ++posts_started;
        }

        // wait until all threads are started. Otherwise, a call to stop_async may stop the pool
        // before all threads are started, and so m_running_worker_threads_count will never
        // reach zero!
        latch_->wait();

        // note: if the ASIO context has nothing to do the pool can stop before we got here!
        auto state_ = state_t::starting;
        m_state.compare_exchange_strong(state_, state_t::running);

        return true;
    } catch (...) {
        // just in case the exception was thrown during the boost::asio::post(...), countback any
        // posts that were not done (so that threads can go past the latch_).
        for (size_t i = posts_started; i < m_no_workers; ++i)
            latch_->count_down();

        if (m_guard)
            m_guard.reset();
        if (!m_context.stopped())
            m_context.stop();
        if (m_pool)
            m_pool->join();

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
    auto state_ = m_state.load(std::memory_order_acquire);
    return (state_ == state_t::running || state_ == state_t::starting);
}

size_t asio_io_pool::worker_count() const { return m_no_workers; }

asio_io_pool::state_t asio_io_pool::state() const {
    return m_state.load(std::memory_order_acquire);
}

void asio_io_pool::stop_async() {
    /* needs mutex so we don't interleave with run() */
    std::lock_guard lock(m_mutex);

    /* don't do anything if are already stopping or stopped */
    auto valState = state_t::running;
    if (!m_state.compare_exchange_strong(valState, state_t::stopping))
        if (valState == state_t::stopping || valState == state_t::stopped)
            return;

    /* note: we can't be in the starting state at this point */
    assert(valState != state_t::starting);

    if (m_guard)
        m_guard.reset();

    if (!m_context.stopped())
        m_context.stop();
}

void asio_io_pool::wait_for_stop() {
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
