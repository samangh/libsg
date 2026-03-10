#include "sg/tcp_client_sync.h"

#include "sg/string.h"
#include "sg/tcp_native.h"

#include <boost/asio.hpp>

namespace {
void throw_error_if_not_timedout(const boost::system::error_code& result_error) {
    if (result_error && result_error != boost::asio::error::operation_aborted)
        SG_THROW(sg::exceptions::net<sg::exceptions::errors::net::other>, result_error.message());
};
}

namespace sg::net {

tcp_client_sync::tcp_client_sync()
    : m_socket(boost::asio::ip::tcp::socket(m_context)) {}
tcp_client_sync::~tcp_client_sync() { disconnect(); };

bool tcp_client_sync::is_connected() const { return (m_socket.is_open()); }
std::string tcp_client_sync::read_until(std::string_view delimiter) {
    if (!is_connected())
        SG_THROW(std::runtime_error, "client not connected");

    /*if previous read does not have delimiter, do new  read */
    if (auto existingPost = m_read_leftover.find(delimiter); existingPost == std::string::npos) {
        std::string result;
        boost::asio::async_read_until(
            m_socket, boost::asio::dynamic_buffer(result), delimiter,
            [&](const boost::system::error_code& result_error, std::size_t) {
                throw_error_if_not_timedout(result_error);
            });

        // will throw if timeout expires
        run(std::chrono::milliseconds{m_options.timeout_msec});

        // add to what was leftover from previous read
        m_read_leftover += result;
    }

    // find position of string to return/keep
    auto pos = m_read_leftover.find(delimiter) + delimiter.length();

    std::string toReturn = m_read_leftover.substr(0, pos);
    m_read_leftover      = m_read_leftover.substr(pos);

    return toReturn;
}
std::string tcp_client_sync::read_some(size_t size) {
    if (!is_connected())
        SG_THROW(std::runtime_error, "client not connected");

    std::string toReturn;

    if (m_read_leftover.length() >= size) {
        toReturn = m_read_leftover.substr(0, size);
        m_read_leftover  = m_read_leftover.substr(size);
    } else {
        std::string result;
        boost::asio::async_read(
            m_socket, boost::asio::dynamic_buffer(result, size - m_read_leftover.length()),
            [&](const boost::system::error_code& result_error, std::size_t) {
                throw_error_if_not_timedout(result_error);
            });
        run(std::chrono::milliseconds{m_options.timeout_msec});

        m_read_leftover += result;

        toReturn = m_read_leftover.substr(0, size);
        m_read_leftover  = m_read_leftover.substr(size);
    }

    return toReturn;
}

void tcp_client_sync::connect(const end_point& endpoint, tcp_session::options_t options) {
    if (is_connected())
        SG_THROW(std::runtime_error, "client already connected");

    m_options = options;

    boost::asio::ip::tcp::resolver resolver(m_context);
    auto endpoints = resolver.resolve(endpoint.ip, std::to_string(endpoint.port));
    boost::asio::async_connect(
        m_socket, endpoints,
        [&](const boost::system::error_code& result_error, const boost::asio::ip::tcp::endpoint&) {
            throw_error_if_not_timedout(result_error);
        });

    run(std::chrono::milliseconds{m_options.timeout_msec});
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

        run(std::chrono::milliseconds{m_options.timeout_msec});
    }
}
void tcp_client_sync::write(const std::byte* data, size_t length) {
    if (!is_connected())
        SG_THROW(std::runtime_error, "client not connected");

    boost::asio::write(m_socket, boost::asio::buffer(data, length));
    boost::asio::async_write(m_socket, boost::asio::buffer(data, length),
                             [&](const boost::system::error_code& result_error, std::size_t) {
                                 throw_error_if_not_timedout(result_error);
                             });
    run(std::chrono::milliseconds{m_options.timeout_msec});
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
void tcp_client_sync::run(std::chrono::steady_clock::duration timeout) {
    /* inspired by
     * https://www.boost.org/doc/libs/latest/doc/html/boost_asio/example/cpp11/timeouts/blocking_tcp_client.cpp*/

    // Restart the io_context, as it may have been left in the "stopped" state
    // by a previous operation.
    m_context.restart();

    try {
        // Block until the asynchronous operation has completed, or timed out. If
        // the pending asynchronous operation is a composed operation, the deadline
        // applies to the entire operation, rather than individual operations on
        // the socket.
        m_context.run_for(timeout);
    } catch (...) {
        m_socket.close();
        m_context.run();
        throw;
    }

    // If the asynchronous operation completed successfully then the io_context
    // would have been stopped due to running out of work. If it was not
    // stopped, then the io_context::run_for call must have timed out.
    //
    // If there was another problem, it would have thrown an exception (see above) and so we
    // wouldn't be here
    if (!m_context.stopped()) {
        m_socket.close();
        m_context.run();
        SG_THROW(exceptions::net<exceptions::errors::net::time_out>, "operation timeout");
    }
}

void tcp_client_sync::write(const shared_c_buffer<std::byte>& msg) { write(msg.get(), msg.size()); }

} // namespace sg::net