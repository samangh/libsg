#pragma once
#include <sg/export/common.h>

#include "callback.h"

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

    void run();
    void restart();
    [[nodiscard]] bool is_running() const;

    void stop_async();
    void wait_for_stop() const;

    [[nodiscard]] bool has_guard() const;

    [[nodiscard]] boost::asio::io_context& context();
    [[nodiscard]] const boost::asio::io_context& context() const;

    [[nodiscard]] static std::shared_ptr<asio_io_pool>
    create(size_t noWorkers, bool enableGuard, stopped_cb_t onStoppedCallBack);

private:
    mutable std::recursive_mutex m_mutex;
    std::unique_ptr<boost::asio::thread_pool> m_pool;
    boost::asio::io_context m_context;

    size_t m_no_workers;
    std::atomic<size_t> m_running_worker_threads_count;

    std::atomic<state_t> m_state{state_t::stopped};

    const bool m_guard_enabled;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_guard;

    const stopped_cb_t m_on_stopped_call_back;
};

}
