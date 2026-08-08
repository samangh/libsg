#include "sg/net/tcp_client_sync.h"
#include "sg/net/tcp_native.h"

#include "sg/string.h"

#include <boost/asio.hpp>

#include <algorithm>
#include <optional>
#include <tuple>

namespace {

/* Runs an asynchronous operation to completion, applying `timeout_msec` to the operation as a
 * whole (0 = don't timeout). The operation is cancelled if the timeout expires, in which case
 * `boost::asio::error::operation_aborted` is reported.
 *
 * `start` must initiate the operation with the completion token it is handed. `Results` are the
 * arguments the operation completes with, after its error code.
 *
 * Returns the operation's results as a tuple, starting with its error code. The results are
 * meaningful even if the operation failed or timed out, so that the caller can hold on to
 * whatever data did arrive. */
template <typename... Results, typename StartOp>
auto run_with_timeout(boost::asio::io_context& context, unsigned timeout_msec, StartOp start) {
    // If we ever have to use Results... that are not default constructable, you can wrap this in an
    // optional
    std::tuple<boost::system::error_code, Results...> result;

    /* completion handler to be passed to each call */
    auto handler = [&result](boost::system::error_code ec, Results... results) {
        result = {ec, std::move(results)...};
    };

    /* the io_context is left in the "stopped" state by the previous operation */
    context.restart();

    if (timeout_msec == 0)
        start(handler);
    else
        start(boost::asio::cancel_after(std::chrono::milliseconds(timeout_msec), handler));

    /* returns once the operation has completed, so `handler` has run by the time this does */
    context.run();

    return result;
}

bool is_timeout(const boost::system::error_code& ec) {
    /* boost::asio::error::timed_out might be raised by the operation itself,
     * boost::asio::error::operation_aborted will be raised by cancel_after */
    return ec == boost::asio::error::timed_out || ec == boost::asio::error::operation_aborted;
}

[[noreturn]] void throw_net_error(const boost::system::error_code& ec) {
    if (is_timeout(ec))
        SG_THROW(sg::exceptions::net::time_out);
    SG_THROW(sg::exceptions::net::other, ec.message());
}

} // namespace

namespace sg::net {

tcp_client_sync::tcp_client_sync()
    : m_socket(boost::asio::ip::tcp::socket(m_context)) {}
tcp_client_sync::~tcp_client_sync() { disconnect(); };

bool tcp_client_sync::is_connected() const { return m_socket.is_open(); }
void tcp_client_sync::throw_on_read_error(const boost::system::error_code& ec) {
    if (!ec)
        return;

    /* the connection is kept on a timeout: the bytes that did arrive are still buffered, and are
     * returned by a later read */
    if (!is_timeout(ec))
        disconnect();

    throw_net_error(ec);
}

std::string tcp_client_sync::read_until(std::string_view delimiter) {
    if (!is_connected())
        SG_THROW(std::runtime_error, "client not connected");

    /* reads straight into m_read_buffer, so that data that arrived before a timeout, as well as
     * data read past the delimiter, stays buffered for a later read */
    const auto [ec, n] =
        run_with_timeout<size_t>(m_context, m_options.timeout_msec, [&](auto token) {
                boost::asio::async_read_until(
                m_socket, boost::asio::dynamic_buffer(m_read_buffer), delimiter, token);
        });

    throw_on_read_error(ec);

    /* n is the number of bytes up to, and including, the delimiter */
    std::string result = m_read_buffer.substr(0, n);
    m_read_buffer.erase(0, n);

    return result;
}

std::string tcp_client_sync::read_some() {
    if (!is_connected())
        SG_THROW(std::runtime_error, "client not connected");

    /* only go to the socket if we have nothing buffered, as we are allowed to return less than
     * what the socket has available */
    if (m_read_buffer.empty()) {
        const auto [ec, n] =
            run_with_timeout<size_t>(m_context, m_options.timeout_msec, [&](auto token) {
                m_socket.async_read_some(boost::asio::buffer(m_read_some_buf), token);
            });

        /* keep hold of whatever did arrive, even if the operation reported an error */
        m_read_buffer.append(&m_read_some_buf[0], n);

        /* only report the error if we have nothing to return, otherwise it will be reported by
         * the next read */
        if (m_read_buffer.empty())
            throw_on_read_error(ec);
    }

    std::string result;
    result.swap(m_read_buffer);
    return result;
}

std::string tcp_client_sync::read(size_t size) {
    if (!is_connected())
        SG_THROW(std::runtime_error, "client not connected");

    if (m_read_buffer.size() < size) {
        /* reads straight into m_read_buffer, so a short read keeps the bytes that did arrive
         * buffered for a later read */
        const auto [ec, n] =
            run_with_timeout<size_t>(m_context, m_options.timeout_msec, [&](auto token) {
                boost::asio::async_read(
                    m_socket, boost::asio::dynamic_buffer(m_read_buffer, size), token);
            });

        throw_on_read_error(ec);
    }

    std::string result = m_read_buffer.substr(0, size);
    m_read_buffer.erase(0, size);
    return result;
}

void tcp_client_sync::connect(const end_point& endpoint, tcp_session::options_t options) {
    if (is_connected())
        SG_THROW(std::runtime_error, "client already connected");

    // Clear left over bytes from previous session
    m_read_buffer.clear();

    m_options = options;

    boost::asio::ip::tcp::resolver resolver(m_context);
    auto endpoints = resolver.resolve(endpoint.ip, std::to_string(endpoint.port));

    const auto [ec, _] = run_with_timeout<boost::asio::ip::tcp::endpoint>(
        m_context, m_options.connection_timeout_msec,
        [&](auto token) { boost::asio::async_connect(m_socket, endpoints, token); });

    if (ec)
        throw_net_error(ec);

    set_keepalive(options.keepalive);
    set_timeout(options.timeout_msec);
}
void tcp_client_sync::disconnect() {
    if (m_socket.is_open()) {
        // in case the client disconnects between the above check and the next command
        try {
            m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        } catch (...) {}

        // need to close, even if the above command failed
        m_socket.close();
    }
}
void tcp_client_sync::write(const std::byte* data, size_t length) {
    if (!is_connected())
        SG_THROW(std::runtime_error, "client not connected");

    const auto ec =
        std::get<0>(run_with_timeout<size_t>(m_context, m_options.timeout_msec, [&](auto token) {
            boost::asio::async_write(m_socket, boost::asio::buffer(data, length), token);
        }));

    if (ec) {
        /* unlike a read, there is nothing to hold on to: a partial write leaves the stream out of
         * sync, so the connection can't be reused */
        disconnect();
        throw_net_error(ec);
    }
}
void tcp_client_sync::write(std::string_view data) {
    write(reinterpret_cast<const std::byte*>(data.data()), data.length());
}
void tcp_client_sync::set_keepalive(keepalive_t keepAlivePa) {
    m_options.keepalive = keepAlivePa;
    sg::net::native::set_keepalive(m_socket.native_handle(), keepAlivePa);
}
void tcp_client_sync::set_timeout(unsigned timeoutMSec) {
    m_options.timeout_msec = timeoutMSec;
    sg::net::native::set_timeout(m_socket.native_handle(), timeoutMSec);
}

void tcp_client_sync::write(const shared_c_buffer<std::byte>& msg) { write(msg.get(), msg.size()); }

} // namespace sg::net