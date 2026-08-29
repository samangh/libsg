#pragma once

#include <sg/export/net.h>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace sg::net {

/** @brief The byte pipe a tcp_session moves data over.
 *
 * A session owns its lifecycle -- state machine, callbacks, write queue, teardown ordering -- and
 * knows nothing about how the bytes are carried. Implement this to add a transport; the session
 * needs no changes.
 *
 * A transport is bound to one socket for life: the session builds it (through a @ref
 * transport_factory) once the socket is settled into place, and hands it that socket then. The
 * transport holds onto it; nothing is threaded through the calls below.
 *
 * Every operation runs on the session's strand and none overlaps another, except that one read and
 * one write may be in flight at once: sessions are full duplex, and implementations must cope.
 */
class SG_NET_EXPORT tcp_transport {
  public:
    tcp_transport() = default;
    virtual ~tcp_transport() = default;

    /* __declspec(dllexport) on a class forces its implicit copy/move assignment to be defined.
     * But some ome transports (e.g. TLS due the ssl::stream<socket&>)don't support it.
     *
     * Make the base non-copyable and non-movable so derived transports' assignment operators are
     * deleted rather than defined. */
    tcp_transport(const tcp_transport&) = delete;
    tcp_transport& operator=(const tcp_transport&) = delete;
    tcp_transport(tcp_transport&&) = delete;
    tcp_transport& operator=(tcp_transport&&) = delete;

    /** Whether this transport negotiates with the peer before it can carry data.
     *
     *  A negotiated transport comes up through @ref handshake() and signs off through
     *  @ref shutdown(). Its session stays out of the @c running state, and so refuses writes, until
     *  the handshake lands. */
    [[nodiscard]] virtual bool is_negotiated_transport_type() const = 0;

    /** Negotiates with the peer. */
    virtual boost::asio::awaitable<void> handshake() = 0;

    /** Reads whatever is available. @throws on failure, end of stream included. */
    virtual boost::asio::awaitable<std::size_t> read_some(boost::asio::mutable_buffer buffer) = 0;

    /** Writes every buffer, giving up after @p timeout_msec (0 = wait indefinitely). Returns the
     *  error rather than throwing: what a failure means depends on what the session was doing.
     *
     *  @p buffers is a reference and these are coroutines, so it is not copied into the frame: it
     *  must outlive the @c co_await, and must not be a temporary. */
    virtual boost::asio::awaitable<boost::system::error_code>
    write_all(const std::vector<boost::asio::const_buffer>& buffers, unsigned timeout_msec) = 0;

    /** Tells the peer the stream ended on purpose.
     *
     *  Best effort and never throws: it runs during teardown, which cannot be allowed to fail. */
    virtual boost::asio::awaitable<void> shutdown() = 0;
};

/** Builds transport for a connection, given the socket it will carry.
 *
 * tcp_server makes one session per accepted connection, so it takes one of these rather than
 * transport. The session calls it with its own socket once that socket is in place. See
 * tls_transport_factory() for the TLS one. */
using transport_factory = std::function<std::unique_ptr<tcp_transport>(boost::asio::ip::tcp::socket&)>;

/** Applies @p factory to @p socket, or gives plain TCP over it if the factory is empty. */
SG_NET_EXPORT std::unique_ptr<tcp_transport> make_transport(const transport_factory& factory,
                                                        boost::asio::ip::tcp::socket& socket);

} // namespace sg::net
