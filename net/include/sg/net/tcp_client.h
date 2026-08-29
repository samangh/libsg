#pragma once
#include "asio_io_pool.h"
#include "net.h"
#include "tcp_session.h"

#include <sg/export/net.h>

namespace sg::net {

class SG_NET_EXPORT tcp_client {
  public:
    tcp_client();
    explicit tcp_client(std::shared_ptr<asio_io_pool> context);
    virtual ~tcp_client();

    /** @param make_transport builds the transport once the socket is connected; plain TCP if
     *         unset. See tls_transport_factory() in sg/net/tls.h for a TLS client. */
    void connect(const end_point& endpoint,
                 tcp_session::Callbacks::OnDataAvailable onReadCb,
                 tcp_session::Callbacks::OnDisconnected onDisconnect,
                 tcp_session::options_t options={},
                 transport_factory make_transport={});
    [[nodiscard]] bool is_connected() const;
    void disconnect();
    /** @warning the returned reference is invalidated by the next connect(), which destroys the
     *  previous session. Do not hold on to it across one. */
    tcp_session& session();
  private:
    // m_session declared after m_context so that m_session is destructed first
    std::shared_ptr<asio_io_pool> m_context;
    std::shared_ptr<tcp_session> m_session;
};

}
