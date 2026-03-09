#include "sg/tcp_client.h"
#include "sg/debug.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace sg::net {

tcp_client::tcp_client() : m_context(asio_io_pool::create()) {}

tcp_client::tcp_client(std::shared_ptr<asio_io_pool> context) : m_context(context) {}

tcp_client::~tcp_client() = default;

void tcp_client::connect(const end_point& endpoint, tcp_session::on_data_available_cb_t onReadCb,
                         tcp_session::on_disconnected_cb_t omDisconnect,
                         tcp_session::options_t options) {
    if (m_session && m_session->is_connected())
        throw std::runtime_error("the client is already connected");

    boost::asio::ip::tcp::socket socket(m_context->context());

    boost::asio::ip::tcp::resolver resolver(m_context->context());
    auto endpoints = resolver.resolve(endpoint.ip, std::to_string(endpoint.port));

    boost::asio::connect(socket, endpoints);

    net::native::set_keepalive(socket.native_handle(), options.keepalive);
    net::native::set_timeout(socket.native_handle(), options.timeout_msec);

    m_session = std::make_unique<tcp_session>(std::move(socket), onReadCb, omDisconnect);
    m_session->start(nullptr);
    m_context->run();
}
bool tcp_client::is_connected() const {
    if (m_session)
        return m_session->is_connected();
    return false;
}
void tcp_client::disconnect() {
    if (is_connected()) {
        m_session->stop_async();
        m_session->wait_until_stopped();
    }
}

tcp_session& tcp_client::session() {
    if (!m_session)
        SG_THROW(std::logic_error, "tcp_session not started");

    return *m_session.get();
}
}
