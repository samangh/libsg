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
      m_on_stopped_call_back(std::move(onStoppedCallBack)) {}

asio_io_pool::~asio_io_pool() {
    stop_async();
    wait_for_stop();
}

std::shared_ptr<asio_io_pool> asio_io_pool::create(size_t noWorkers, bool enableGuard,
                                                   stopped_cb_t onStoppedCallBack) {
    return std::make_shared<asio_io_pool>(Private(), noWorkers, enableGuard, onStoppedCallBack);
}

void asio_io_pool::run() {
    auto valState = state_t::stopped;
    if (!m_state.compare_exchange_strong(valState, state_t::starting)) {
        if (valState == state_t::starting || valState == state_t::running)
            return;
        SG_THROW(std::runtime_error, "asio_io_pool is in not an appropriate state for run()");
    }

    m_context.restart();
    if (m_guard_enabled) {
        m_guard = std::make_unique<
            boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            boost::asio::make_work_guard(m_context));
    }

    // We set the connection count before, because if the io_context has nothing to do the on_stop
    // handlers may get called before we have started all of them.
    m_running_worker_threads_count.store(m_no_workers);

    /* wait until the context is running before returning! */
    std::binary_semaphore contextRunning{0};
    boost::asio::post(m_context, [&contextRunning](){contextRunning.release();});

    std::latch latch_(m_no_workers);
    m_pool = std::make_unique<boost::asio::thread_pool>(m_no_workers);

    for (size_t i = 0; i < m_no_workers; ++i)
        boost::asio::post(m_pool->executor(), [this, &latch_]() {
            latch_.count_down();

            m_context.run();

            m_state.store(state_t::stopping, std::memory_order_release);

            /* in case this is the last one */
            if (--m_running_worker_threads_count == 0) {
                m_pool->stop();
                m_state.store(state_t::stopped, std::memory_order_release);
                if (m_on_stopped_call_back)
                    m_on_stopped_call_back.invoke(*this);
            }
        });

    // wait until all threads are started. If this pool is stopped before all threads are stopped,
    // m_on_stopped_call_back will never reach zero!
    latch_.wait();
    contextRunning.acquire();

    m_state.store(state_t::running, std::memory_order_release);
}

boost::asio::io_context& asio_io_pool::context() { return m_context; }

const boost::asio::io_context& asio_io_pool::context() const {
    return m_context;
}

bool asio_io_pool::is_running() const {
    auto state_ = m_state.load(std::memory_order_acquire);
    return (state_ == state_t::running || state_ == state_t::starting);
}


void asio_io_pool::stop_async() {
    /* don't do anything if are already stopping or stopped */
    auto valState = state_t::running;
    while (true) {
        if (m_state.compare_exchange_strong(valState, state_t::stopping))
            break;

        if (valState == state_t::stopping || valState == state_t::stopped)
            return;
    }

    m_pool->stop();

    if (m_guard)
        m_guard.get()->reset();

    if (!m_context.stopped())
        m_context.stop();
}

void asio_io_pool::wait_for_stop() const {
    std::lock_guard lock(m_mutex);

    if (m_pool)
        m_pool->join();
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
