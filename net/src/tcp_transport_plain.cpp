#include "sg/net/tcp_transport_plain.h"

#include "transport_timeout.h"

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

namespace sg::net {

boost::asio::awaitable<void> tcp_transport_plain::handshake() { co_return; }

boost::asio::awaitable<void> tcp_transport_plain::shutdown() noexcept { co_return; }

boost::asio::awaitable<std::size_t> tcp_transport_plain::read_some(boost::asio::mutable_buffer buffer) {
    co_return co_await m_socket.async_read_some(buffer, boost::asio::use_awaitable);
}

boost::asio::awaitable<boost::system::error_code>
tcp_transport_plain::write_all(const std::vector<boost::asio::const_buffer>& buffers,
                         unsigned timeout_msec) {
    co_return co_await detail::with_timeout(
        [&](auto token) { return boost::asio::async_write(m_socket, buffers, token); },
        timeout_msec);
}

} // namespace sg::net
