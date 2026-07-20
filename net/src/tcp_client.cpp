#include "sg/net/tcp_client.h"

#include "sg/debug.h"

#include <boost/asio.hpp>

#include <optional>

namespace sg::net {

tcp_client::tcp_client() =default;
tcp_client::tcp_client(std::shared_ptr<asio_io_pool> context) : m_context(std::move(context)) {}

tcp_client::~tcp_client() {
    /* if the asio_io_pool was supplied externally, then it may carry on running after this client
     * is disconnected. This means that the tcp_session won't disconnect, as the reader/writer
     * coroutines will have a shared_ptr to the tcp_session.
     *
     * so ensure that we disconnect if tcp_client is destructed */

    /* to prevent deadlock in cast somehow a callback from the session is calling this destructor,
     * don't wait for session to stop from the session IO thread! */
    if (m_session) {
        if (m_session->running_in_io_thread())
            m_session->stop_async();
        else
            disconnect();
    }
};

void tcp_client::connect(const end_point& endpoint, tcp_session::Callbacks::OnDataAvailable onReadCb,
                         tcp_session::Callbacks::OnDisconnected onDisconnect,
                         tcp_session::options_t options) {
    /* The user might call connect() from the OnDisconnected callback, but that's a bad idea as it
     * can cause a deadlock */
    if (m_session && m_session->running_in_io_thread())
        SG_THROW(std::logic_error,
                 "tcp_client::connect() must not be called from the I/O thread (e.g. from a callback)");

    if (m_session && m_session->is_connected())
        throw std::runtime_error("the client is already connected");

    if (m_session) {
        m_session->stop_async();
        m_session->wait_until_stopped();
        m_session.reset();
    }

    /* an externally passed asio_pool will always have it's guard enabled */
    if (!m_context)
        m_context = asio_io_pool::create(1, false, nullptr);

    boost::asio::ip::tcp::socket socket(m_context->context());
    boost::asio::ip::tcp::resolver resolver(m_context->context());
    const auto endpoints = resolver.resolve(endpoint.ip, std::to_string(endpoint.port));

    /* ensure tha the context is running (whether it's an internal or external)
     * the work_guard will get destructed as we go out of scope  */
    auto work = boost::asio::make_work_guard(m_context->context());
    m_context->run();

    /* the coroutine must only do the connect itself: tcp_session::start() has a failure path
     * that blocks in wait_until_stopped() until a handler dispatched to the pool runs, so
     * calling it on the pool's (single) worker thread would deadlock. It is run on this thread
     * instead, after fut.get() has freed the worker. */
    auto fut = boost::asio::co_spawn(
        m_context->context(),
        [&]() -> boost::asio::awaitable<void> {
            /* cancel_after cancels the connect once the timeout elapses; as_tuple delivers
             * the error code as a value */
            auto result = co_await boost::asio::async_connect(
                socket, endpoints,
                boost::asio::cancel_after(std::chrono::milliseconds(options.timeout_msec),
                                          boost::asio::as_tuple(boost::asio::use_awaitable)));

            if (auto ec = std::get<0>(result); ec) {
                if (ec == boost::asio::error::timed_out ||
                    ec == boost::asio::error::operation_aborted)
                    SG_THROW(exceptions::net::time_out);
                throw boost::system::system_error(ec);
            }
        },
        boost::asio::use_future);

    fut.get();

    m_session = tcp_session::create(std::move(socket),
                                    tcp_session::Callbacks{.onConnected     = nullptr,
                                                           .onDisconnected  = onDisconnect,
                                                           .onDataAvailable = onReadCb},
                                    options);
    try {
        m_session->start();
    } catch (...) {
        /* start() has already stopped the session; don't leave a dead session exposed via
         * session() / is_connected() */
        m_session.reset();
        throw;
    }
}

bool tcp_client::is_connected() const {
    if (m_session)
        return m_session->is_connected();
    return false;
}
void tcp_client::disconnect() {
    if (m_session && m_session->running_in_io_thread())
        SG_THROW(std::logic_error,
                 "tcp_client::connect() must not be called from the I/O thread (e.g. from a callback)");

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
