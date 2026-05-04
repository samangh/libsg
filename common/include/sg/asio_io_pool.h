#pragma once
#include <sg/export/common.h>

#include "callback.h"
#include "jthread.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>

namespace sg::net {

class SG_COMMON_EXPORT asio_io_pool {
    struct Private{ explicit Private() = default; };

  public:
    enum class state_t {stopped, starting, running, stopping};
    CREATE_CALLBACK(stopped_cb_t, void, asio_io_pool&)

    asio_io_pool(Private, size_t noWorkers, bool enableGuard, stopped_cb_t onStoppedCallBack);
    ~asio_io_pool();

    /**
     * Starts running the io_context pool. If the pool is already running, this is a no-op.
     *
     * Note that this does not clear any handlers or actions are already queued on the ASIO
     * io_context. So you can post things to the io_context, and they will run after calling this
     * function.
     *
     * If the io_context pool is in middle of stopping, this will block until the pool has stopped
     * and then run it again.
     *
     * @return true if the io_context had to be started, false if it was already running.
     */
    bool run();
    void restart();

    void stop_async();
    /** note: this can wait forever if stop_async() is not called and the pool has a guard */
    void wait_for_stop();

    [[nodiscard]] bool has_guard() const;
    [[nodiscard]] bool is_running() const;
    [[nodiscard]] size_t worker_count() const;
    [[nodiscard]] state_t state() const;

    [[nodiscard]] boost::asio::io_context& context();
    [[nodiscard]] const boost::asio::io_context& context() const;

    [[nodiscard]] static std::shared_ptr<asio_io_pool>
    create(size_t noWorkers, bool enableGuard, stopped_cb_t onStoppedCallBack);

private:
    mutable std::mutex m_mutex;
    std::unique_ptr<boost::asio::thread_pool> m_pool;
    boost::asio::io_context m_context;

    const size_t m_no_workers;
    std::atomic<size_t> m_running_worker_threads_count{0};

    std::atomic<state_t> m_state{state_t::stopped};

    const bool m_guard_enabled;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_guard;

    const stopped_cb_t m_on_stopped_call_back;
    std::jthread m_cb_thread;
};

}
