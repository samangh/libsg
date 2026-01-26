#include "sg/tcp_client.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace sg::net {

tcp_client::tcp_client() : m_context(tcp_context::create()) {}

tcp_client::tcp_client(std::shared_ptr<tcp_context> context) : m_context(context) {}

tcp_client::~tcp_client() {
    if (m_session) {
        m_session->stop_async();
        m_session->wait_until_stopped();
    }
}

void tcp_client::connect(const end_point& endpoint,
                         tcp_session::on_data_available_cb_t onReadCb,
                         tcp_session::on_disconnected_cb_t omDisconnect) {
    if (m_session && m_session->is_connected())
        throw std::runtime_error("the client is already connected");

    boost::asio::ip::tcp::socket socket(m_context->context());

    boost::asio::ip::tcp::resolver resolver(m_context->context());
    auto endpoints = resolver.resolve(endpoint.ip, std::to_string(endpoint.port));
    boost::asio::connect(socket, endpoints);

    m_session = std::make_shared<tcp_session>(std::move(socket), onReadCb, omDisconnect);
    m_session->start();
    m_context->run();
}

tcp_session& tcp_client::session() { return *m_session.get(); }
}
