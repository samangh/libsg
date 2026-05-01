#include "sg/tcp_client.h"

#include "sg/debug.h"

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace sg::net {

tcp_client::tcp_client() {
}

tcp_client::tcp_client(std::shared_ptr<asio_io_pool> context) : m_context(std::move(context)) {
    if (!m_context->has_guard())
        SG_THROW(std::runtime_error, "asio_io_pool does not have guard enabled, it must be enabled for share ASIO pools");
}
tcp_client::~tcp_client() = default;

void tcp_client::connect(const end_point& endpoint, tcp_session::on_data_available_cb_t onReadCb,
                         tcp_session::on_disconnected_cb_t onDisconnect,
                         tcp_session::options_t options) {
    if (m_session && m_session->is_connected())
        throw std::runtime_error("the client is already connected");

    if (m_session) {
        m_session->stop_async();
        m_session->wait_until_stopped();
        m_session.reset();
    }

    /* an externally passed asio_pool will alays have it's guard enabled */
    if (!m_context || !m_context->has_guard())
        m_context = asio_io_pool::create(1, false, nullptr);

    boost::asio::ip::tcp::socket socket(m_context->context());
    boost::asio::ip::tcp::resolver resolver(m_context->context());
    const auto endpoints = resolver.resolve(endpoint.ip, std::to_string(endpoint.port));

    {
        using namespace boost::asio::experimental::awaitable_operators;

        auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(options.timeout_msec);

        auto fut = boost::asio::co_spawn(
            m_context->context(),
            [&]() -> boost::asio::awaitable<void> {
                auto result = co_await (
                    boost::asio::async_connect(socket, endpoints, boost::asio::use_awaitable) ||
                    async_timeout(deadline));
                if (result.index() == 1)
                    SG_THROW(exceptions::net<exceptions::errors::net::time_out>, "operation timeout");

                m_session = tcp_session::create(std::move(socket), onReadCb, onDisconnect, options);
                m_session->start(nullptr);
            },
            boost::asio::use_future);

        m_context->run();
        fut.get();
    }
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
boost::asio::awaitable<void>
tcp_client::async_timeout(std::chrono::steady_clock::time_point& deadline) const {
    boost::asio::steady_timer timer(m_context->context());
    auto now = std::chrono::steady_clock::now();
    while (deadline > now) {
        timer.expires_at(deadline);
        co_await timer.async_wait(boost::asio::use_awaitable);
        now = std::chrono::steady_clock::now();
    }
    // m_context->context().stop();
    // throw boost::system::system_error(std::make_error_code(std::errc::timed_out));
}
} // namespace sg::net
