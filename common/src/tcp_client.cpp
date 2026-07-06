#include "sg/tcp_client.h"

#include "sg/debug.h"

#include <boost/asio.hpp>

#include <optional>

namespace sg::net {

tcp_client::tcp_client() {
}

tcp_client::tcp_client(std::shared_ptr<asio_io_pool> context) : m_context(std::move(context)) {
    if (!m_context->has_guard())
        SG_THROW(std::runtime_error, "asio_io_pool does not have guard enabled, it must be enabled for share ASIO pools");
}
tcp_client::~tcp_client() {
    /* if the asio_io_pool was supplied externally, then it may carry on running after this client
     * is disconnected. This means that the tcp_session won't disconnect, as the reader/writer
     * coroutines will have a shared_ptr to the tcp_session.
     *
     * so ensure that we disconnect if tcp_client is destructed */
    disconnect();
};

void tcp_client::connect(const end_point& endpoint, tcp_session::Callbacks::OnDataAvailable onReadCb,
                         tcp_session::Callbacks::OnDisconnected onDisconnect,
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

    /* an internally-created pool is guardless: it stops once it runs out of work. Between the
     * connect coroutine finishing and start() spawning the reader the context has no work, so
     * without this guard the pool would drain in that gap and the reader would be posted to a
     * stopped context, never to run. Released on scope exit (including exception unwind). */
    auto work = boost::asio::make_work_guard(m_context->context());

    /* The coroutine must own everything it touches (socket moved in, endpoints copied): if
     * m_context->run() throws, connect() unwinds while the coroutine is still queued — and on a
     * shared pool the owner may run the context later, executing the coroutine after this frame is
     * gone.*/
    auto fut = boost::asio::co_spawn(
        m_context->context(),
        /* returns optional<socket> rather than socket: co_spawn's exception path
         * default-constructs the result type, and sockets are not default-constructible */
        [socket = std::move(socket), endpoints, timeoutMSec = options.timeout_msec]() mutable
            -> boost::asio::awaitable<std::optional<boost::asio::ip::tcp::socket>> {
            /* cancel_after cancels the connect once the timeout elapses; as_tuple delivers
             * the error code as a value */
            auto result = co_await boost::asio::async_connect(
                socket, endpoints,
                boost::asio::cancel_after(std::chrono::milliseconds(timeoutMSec),
                                          boost::asio::as_tuple(boost::asio::use_awaitable)));

            if (auto ec = std::get<0>(result); ec) {
                if (ec == boost::asio::error::operation_aborted)
                    SG_THROW(exceptions::net::time_out);
                throw boost::system::system_error(ec);
            }

            co_return std::move(socket);
        },
        boost::asio::use_future);

    m_context->run();

    /* create and start the session on the caller's thread, not the pool: if start() throws,
     * its cleanup blocks waiting for a close that is dispatched to the pool, which would
     * deadlock a single-worker pool if we were occupying that worker */
    m_session = tcp_session::create(std::move(*fut.get()),
                                    tcp_session::Callbacks{.onConnected     = nullptr,
                                                           .onDisconnected  = onDisconnect,
                                                           .onDataAvailable = onReadCb},
                                    options);
    m_session->start();
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
} // namespace sg::net
