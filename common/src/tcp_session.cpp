#include "sg/tcp_session.h"
#include "sg/tcp_native.h"

#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/write.hpp>

namespace sg::net {

tcp_session::tcp_session(boost::asio::ip::tcp::socket socket,
                         on_data_available_cb_t onReadCb,
                         on_disconnected_cb_t onErrorCb)
    : m_socket(std::move(socket)),
      m_timer(m_socket.get_executor()),
      m_on_data_cb(std::move(onReadCb)),
      m_on_disconnected_cb(std::move(onErrorCb)) {
    m_timer.expires_at(std::chrono::steady_clock::time_point::max());

}

tcp_session::~tcp_session() {
    // this causes teh right call-backs to be called, if they haven't already
    close({});
}

void tcp_session::start() {
    co_spawn(
        m_socket.get_executor(),
        [self = shared_from_this()] { return self->reader(); },
        boost::asio::detached);
    co_spawn(
        m_socket.get_executor(),
        [self = shared_from_this()] { return self->writer(); },
        boost::asio::detached);

    m_stopped.store(false);
}

void tcp_session::write(sg::shared_c_buffer<std::byte> msg) {
    if (m_stop_requested.load(std::memory_order::acquire))
        throw std::runtime_error(
            "attempt to write to tcp_session after a disconnection was requested");

    m_write_msgs.push_back(std::move(msg));
    m_timer.cancel_one();
}

void tcp_session::write(std::string_view msg) {
    auto buff = sg::make_shared_c_buffer<std::byte>(msg.size());
    std::memcpy(buff.get(), msg.data(), msg.size());
    write(buff);
}

void tcp_session::set_keepalive(bool enableKeepAlive, unsigned idleSec, unsigned intervalSec,
                                unsigned count) {
    sg::net::native::set_keepalive(m_socket.native_handle(), enableKeepAlive, idleSec, intervalSec,
                                   count);
}

void tcp_session::set_timeout(unsigned timeoutMSec) {
    sg::net::native::set_timeout(m_socket.native_handle(), timeoutMSec);
}

void tcp_session::stop_async() {
    // No action if we have already stopped
    if (m_stopped.load(std::memory_order::acquire))
        return;

    m_stop_requested.store(true, std::memory_order::release);
    m_timer.cancel_one();
}

void tcp_session::wait_until_stopped() const {
    // block until m_stopped is true
    m_stopped.wait(false, std::memory_order::acquire);
}

bool tcp_session::is_connected() const {
    return !m_stopped.load(std::memory_order::acquire);
}

end_point tcp_session::local_endpoint() {
    auto asioEp = m_socket.local_endpoint();
    return sg::net::end_point(asioEp.address().to_string(), asioEp.port());
}

end_point tcp_session::remote_endpoint() {
    auto asioEp = m_socket.remote_endpoint();
    return sg::net::end_point(asioEp.address().to_string(), asioEp.port());
}

void tcp_session::close(std::optional<std::exception> ex) {
    if (m_disconnected_cb_called.exchange(true))
        return;

    if (m_on_disconnected_cb)
        m_on_disconnected_cb(ex);

    try {
        if (m_socket.is_open())
            m_socket.close();
    } catch (...) {}
    m_timer.cancel();

    m_stopped.store(true, std::memory_order::release);
    m_stopped.notify_all(); // needed for atomic wait()
}

boost::asio::awaitable<void> tcp_session::reader() {
    try {
        boost::asio::socket_base::receive_buffer_size option;
        m_socket.get_option(option);
        int size = option.value();

        auto data = std::make_unique<std::byte[]>(size);
        while (true) {
            std::size_t n = co_await m_socket.async_read_some(boost::asio::buffer(data.get(), size),
                                                              boost::asio::use_awaitable);
            if (m_on_data_cb)
                m_on_data_cb(data.get(), n);
        }
    } catch (const std::exception& ex) {
        /* if clean closing, do not throw error */
        if (m_stop_requested.load(std::memory_order::acquire))
            close({});
        else
            close(ex);
    }

    co_return;
}

boost::asio::awaitable<void> tcp_session::writer() {
    try {
        while (m_socket.is_open()) {
            /* if shutdown is requested, then wait until all message is written
             *
             * Once the shutdown request is sent, the reader() will throw an error which will cause
             * stop(ex) to be called.
             */
            if (m_stop_requested.load(std::memory_order::acquire) && m_write_msgs.empty()) {
                m_socket.shutdown(m_socket.shutdown_both);
                co_return;
            }

            if (m_write_msgs.empty()) {
                boost::system::error_code ec;
                co_await m_timer.async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            } else {
                auto front = m_write_msgs.pop_front();
                co_await boost::asio::async_write(m_socket,
                                                  boost::asio::buffer(front->get(), front->size()),
                                                  boost::asio::use_awaitable);
            }
        }
    } catch (const std::exception& ex) { close(ex); }
    co_return;
}
}
