#include "sg/net/tcp_transport_tls.h"

#include "sg/debug.h"
#include "sg/net/net.h"
#include "sg/net/tcp_transport.h"
#include "sg/net/tls.h"
#include "transport_timeout.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <stdexcept>

namespace {

boost::asio::ssl::context& checked_context(const sg::net::tls_config& config) {
    if (!config.context)
        SG_THROW(std::invalid_argument, "sg::net::tls_config has no ssl::context");
    return *config.context;
}

} // namespace

namespace sg::net {

tls_transport::tls_transport(boost::asio::ip::tcp::socket& socket, tls_config config)
: m_config(std::move(config)), m_stream(socket, checked_context(m_config)) {
    /* Only client verify's hostname */
    if (m_config.role != tls_role::client || m_config.hostname.empty())
        return;

    /* One address can serve certificates for many names; without SNI the peer has to guess which
     * of them we came for. */
    if (!SSL_set_tlsext_host_name(m_stream.native_handle(), m_config.hostname.c_str()))
        throw boost::system::system_error(
            boost::system::error_code(static_cast<int>(::ERR_get_error()),
                                      boost::asio::error::get_ssl_category()));

    /* verify hostname */
    m_stream.set_verify_callback(boost::asio::ssl::host_name_verification(m_config.hostname));
}

boost::asio::awaitable<void> tls_transport::handshake() {
    const auto role = m_config.role == tls_role::server ? boost::asio::ssl::stream_base::server
                                                        : boost::asio::ssl::stream_base::client;

    const auto ec = co_await detail::with_timeout(
        [&](auto token) { return m_stream.async_handshake(role, token); },
        m_config.handshake_timeout_msec);

    if (!ec)
        co_return;

    if (ec == boost::asio::error::operation_aborted)
        SG_THROW(exceptions::net::time_out);

    throw boost::system::system_error(ec);
}

boost::asio::awaitable<std::size_t> tls_transport::read_some(boost::asio::mutable_buffer buffer) {
    co_return co_await m_stream.async_read_some(buffer, boost::asio::use_awaitable);
}

boost::asio::awaitable<boost::system::error_code>
tls_transport::write_all(const std::vector<boost::asio::const_buffer>& buffers,
                         unsigned timeout_msec) {
    /* Safe to run while a read is in flight only because the session drives both directions from
     * one strand: OpenSSL does not lock the SSL object */
    co_return co_await detail::with_timeout(
        [&](auto token) { return boost::asio::async_write(m_stream, buffers, token); },
        timeout_msec);
}

boost::asio::awaitable<void> tls_transport::shutdown() {
    /* Send close_notify, so the peer can tell this was a real end of stream and not someone
     * cutting the connection.
     *
     * Left alone, async_shutdown() would also wait to read the peer's close_notify back, which
     * cannot work here: the session's reader owns the read half, so the shutdown contends with it
     * for ssl::stream's internal read mutex and sits out the whole timeout. Declaring the receive
     * direction already shut down makes this a pure write, needing nothing from the reader.
     *
     * Nothing is lost by not reading theirs: read_some() reports how the peer's side ended, and
     * already tells a close_notify (eof) from a truncation (ssl::error::stream_truncated). */
    ::SSL_set_shutdown(m_stream.native_handle(), SSL_RECEIVED_SHUTDOWN);

    /* The socket is going regardless, so a failure here has nowhere useful to go. */
    co_await detail::with_timeout([&](auto token) { return m_stream.async_shutdown(token); },
                                  m_config.shutdown_timeout_msec);
}

transport_factory tls_transport_factory(tls_config config) {
    return [config = std::move(config)](boost::asio::ip::tcp::socket& socket) {
        return std::unique_ptr<tcp_transport>(std::make_unique<tls_transport>(socket, config));
    };
}

} // namespace sg::net
