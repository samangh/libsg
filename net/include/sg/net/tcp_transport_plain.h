#pragma once

#include "tcp_transport.h"

namespace sg::net {

/** Plain TCP: the socket is the stream, with nothing to negotiate either way and no state. */
class SG_NET_EXPORT tcp_transport_plain final : public tcp_transport {
  public:
    explicit tcp_transport_plain(boost::asio::ip::tcp::socket& socket) : m_socket(socket) {}

    [[nodiscard]] bool is_negotiated() const override { return false; }

    boost::asio::awaitable<void> handshake() override;
    boost::asio::awaitable<void> shutdown() override;
    boost::asio::awaitable<std::size_t> read_some(boost::asio::mutable_buffer buffer) override;
    boost::asio::awaitable<boost::system::error_code>
    write_all(const std::vector<boost::asio::const_buffer>& buffers, unsigned timeout_msec) override;

  private:
    boost::asio::ip::tcp::socket& m_socket;
};

} // namespace sg::net
