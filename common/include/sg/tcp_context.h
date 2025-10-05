#pragma once

#include <sg/export/common.h>

#include "notifiable_background_worker.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>

namespace sg::net {

class SG_COMMON_EXPORT tcp_context : public std::enable_shared_from_this<tcp_context> {
    struct Private{ explicit Private() = default; };

  public:
    tcp_context(Private);

    void run();
    bool is_running() const;

    boost::asio::io_context& context();
    const boost::asio::io_context& context() const;

    static std::shared_ptr<tcp_context> create();
  private:
    boost::asio::io_context m_io_context_ptr;
    std::unique_ptr<notifiable_background_worker> m_worker;

    void on_worker_tick(notifiable_background_worker* worker);
};

}
