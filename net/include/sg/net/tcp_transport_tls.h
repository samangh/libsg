#pragma once
#ifdef LIBSG_NET_TLS

#include "tcp_transport.h"
#include "tls.h"

namespace sg::net {

/** @brief TLS over the session's socket.
 *
 * Every byte handed to @c tcp_session::write() is encrypted on the wire, and everything reported
 * through @c OnDataAvailable has been decrypted.
 *
 * One instance per connection: it holds that connection's TLS state.
 *
 * @throws std::invalid_argument from the constructor if @p config has no @c ssl::context
 * @throws boost::system::system_error from the constructor if OpenSSL rejects the per-connection
 *         setup (SNI, hostname verification)
 */
class SG_NET_EXPORT tls_transport final : public tcp_transport {
  public:
    /** @p socket is the connection this wraps; it must outlive the transport, which the session
     *  guarantees by owning both and destroying the transport first. */
    tls_transport(boost::asio::ip::tcp::socket& socket, tls_config config);

    [[nodiscard]] bool is_negotiated_transport_type() const override { return true; }

    boost::asio::awaitable<void> handshake() override;
    boost::asio::awaitable<void> shutdown() noexcept override;
    boost::asio::awaitable<std::size_t> read_some(boost::asio::mutable_buffer buffer) override;
    boost::asio::awaitable<boost::system::error_code>
    write_all(const std::vector<boost::asio::const_buffer>& buffers, unsigned timeout_msec) override;

  private:
    tls_config m_config;
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket&> m_stream;
};

/** A @ref transport_factory that gives each connection a tls_transport with @p config. */
SG_NET_EXPORT transport_factory tls_transport_factory(tls_config config);

} // namespace sg::net

#endif
