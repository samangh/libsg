#include "sg/tcp_session.h"

#include <boost/asio/redirect_error.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/detached.hpp>

#if defined(_WIN32)
#include <Winsock2.h>
#include <Mswsock.h>
#endif

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
    if (!m_disconnected_cb_called.exchange(true) && m_on_disconnected_cb)
        m_on_disconnected_cb({});
    m_stopped.release();
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
}

void tcp_session::write(sg::shared_c_buffer<std::byte> msg) {
    if (m_stop_requested.load(std::memory_order::acquire))
        throw std::runtime_error(
            "attemp to write to tcp_session after a disconnection was requested");

    m_write_msgs.push_back(std::move(msg));
    m_timer.cancel_one();
}

void tcp_session::write(std::string_view msg) {
    auto buff = sg::make_shared_c_buffer<std::byte>(msg.size());
    std::memcpy(buff.get(), msg.data(), msg.size());
    write(buff);
}

void tcp_session::set_keepalive(bool enableKeepAlive, uint32_t idleSec, uint32_t intervalSec,
                                uint32_t count) {
    const boost::asio::socket_base::keep_alive option(enableKeepAlive);
    m_socket.set_option(option);

    if (enableKeepAlive) {
        auto nativeHndl = m_socket.native_handle();

        // mNote: macOS has a separate option, but for Windows/Linux the only difference is the data
        // type for the times

#if defined(__APPLE__)
        setsockopt(nativeHndl, IPPROTO_TCP, TCP_KEEPALIVE, &idleSec, sizeof(idleSec));
#elif defined(_WIN32)
        setsockopt(nativeHndl, IPPROTO_TCP, TCP_KEEPIDLE, (const char*)&idleSec, sizeof(idleSec));
        setsockopt(nativeHndl, IPPROTO_TCP, TCP_KEEPINTVL, (const char*)&intervalSec, sizeof(intervalSec));
        setsockopt(nativeHndl, IPPROTO_TCP, TCP_KEEPCNT, (const char*)&count, sizeof(count));
#else
        setsockopt(nativeHndl, IPPROTO_TCP, TCP_KEEPIDLE, &idleSec, sizeof(idleSec));
        setsockopt(nativeHndl, IPPROTO_TCP, TCP_KEEPINTVL, &intervalSec, sizeof(intervalSec));
        setsockopt(nativeHndl, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#endif
    }
}

void tcp_session::set_timeout(uint32_t timeoutMSec) {
    auto nativeHndl = m_socket.native_handle();

#if defined(_WIN32)
    setsockopt(nativeHndl, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMSec, sizeof(timeoutMSec));
    setsockopt(nativeHndl, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutMSec, sizeof(timeoutMSec));
#else
    // assume everything else is posix
    struct timeval tv;
    tv.tv_sec  = timeoutMSec / 1000;
    tv.tv_usec = (timeoutMSec % 1000) * 1000;
    setsockopt(nativeHndl, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(nativeHndl, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

void tcp_session::stop_async() {
    m_stop_requested.store(true, std::memory_order::release);
    m_timer.cancel_one();
}

void tcp_session::wait_until_stopped() const {
    m_stopped.acquire();
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
        m_socket.close();
    } catch (...) {}
    m_timer.cancel();

    m_stopped.release();
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
