#pragma once
#include <sg/export/common.h>

#include "net.h"
#include "notifiable_background_worker.h"
#include "tcp_session.h"
#include "tcp_context.h"

namespace sg::net {

class SG_COMMON_EXPORT tcp_client {
  public:
    tcp_client();
    explicit tcp_client(std::shared_ptr<tcp_context> context);
    ~tcp_client();

    void connect(const end_point& endpoint,
                 tcp_session::on_data_available_cb_t onReadCb,
                 tcp_session::on_disconnected_cb_t omDisconnect);

    tcp_session& session();
  private:
    std::shared_ptr<tcp_context> m_context;
    std::shared_ptr<tcp_session> m_session;
};

}
