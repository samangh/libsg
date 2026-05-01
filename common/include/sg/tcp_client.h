#pragma once
#include "asio_io_pool.h"
#include "net.h"
#include "notifiable_background_worker.h"
#include "tcp_session.h"

#include <sg/export/common.h>

namespace sg::net {

class SG_COMMON_EXPORT tcp_client {
  public:
    tcp_client();
    explicit tcp_client(std::shared_ptr<asio_io_pool> context);
    virtual ~tcp_client();

    void connect(const end_point& endpoint,
                 tcp_session::on_data_available_cb_t onReadCb,
                 tcp_session::on_disconnected_cb_t onDisconnect,
                 tcp_session::options_t options={});
    [[nodiscard]] bool is_connected() const;
    void disconnect();
    tcp_session& session();
  private:
    // m_session declared after m_context so that m_session is destructed first
    std::shared_ptr<asio_io_pool> m_context;
    std::shared_ptr<tcp_session> m_session;

    boost::asio::awaitable<void> async_timeout(std::chrono::steady_clock::time_point& deadline) const;
};

}
