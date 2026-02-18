#pragma once

#include <sg/export/common.h>

#include "notifiable_background_worker.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

namespace sg::net {

class SG_COMMON_EXPORT tcp_context {
    struct Private{ explicit Private() = default; };

  public:
    typedef std::function<void(tcp_context&)> stopped_cb_t;

    tcp_context(Private, size_t noWorkers, stopped_cb_t onStoppedCallBack);
    virtual ~tcp_context();

    void run(bool enableGuard = false);
    void stop_async();
    void wait_for_stop();

    void reset_guard();

    [[nodiscard]] bool is_running() const;
    void future_get_once() noexcept(false);

    [[nodiscard]] boost::asio::io_context& context();
    [[nodiscard]] const boost::asio::io_context& context() const;

    [[nodiscard]] static std::shared_ptr<tcp_context> create(size_t noWorkers=1, stopped_cb_t onStoppedCallBack=nullptr);
  private:
    std::atomic<size_t> m_running_worker_threads_count;

    boost::asio::io_context m_io_context_ptr;
    std::vector<std::unique_ptr<notifiable_background_worker>> m_workers;

    stopped_cb_t m_on_stopped_call_back;

    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_guard;
    void on_worker_tick(notifiable_background_worker* worker);
    void on_worker_stop(notifiable_background_worker*);
};

}
