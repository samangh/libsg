#include "sg/tcp_session.h"
#include "sg/tcp_native.h"
#include "sg/debug.h"

#include <boost/asio/detached.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace sg::net {

tcp_session::tcp_session(boost::asio::ip::tcp::socket socket, on_data_available_cb_t onReadCb,
                         on_disconnected_cb_t onErrorCb, options_t options)
    : m_socket(std::move(socket)),
      m_on_data_cb(std::move(onReadCb)),
      m_on_disconnected_cb(std::move(onErrorCb)),
      m_options(options) {
}

tcp_session::~tcp_session() {
    // this causes the right call-backs to be called, if they haven't already
    stop_async();
    wait_until_stopped();
}

void tcp_session::start(on_connected_cb_t onConn) {
    try {
        m_stopped.store(false);

        if (onConn)
            onConn.invoke(*this);

        // note: below can throw if the client has disconnected
        set_keepalive(m_options.keepalive);
        set_timeout(m_options.timeout_msec);

        m_reader_running.store(true);
        co_spawn(m_socket.get_executor(), [this] { return reader(); }, boost::asio::detached);
    } catch (...) {
        /* if clean closing, do not throw error */
        if (!m_stop_requested.load(std::memory_order::acquire))
        {
            std::lock_guard lock(m_exception_mutex);
            if (!m_exception)
                m_exception = std::current_exception();
        }

        stop_async();
        close();
    }
}

void tcp_session::write(sg::shared_c_buffer<std::byte> msg) {
    if (m_stop_requested.load(std::memory_order::acquire))
        SG_THROW(std::runtime_error,
                 "attempt to write to tcp_session after a disconnection was requested");

    bool need_spawn = false;
    {
        std::lock_guard lock(m_write_mutex);
        m_write_msgs.push_back(std::move(msg));
        need_spawn = !std::exchange(m_write_scheduled, true);
    }

    if (need_spawn)
        co_spawn(m_socket.get_executor(), [this] { return writer(); }, boost::asio::detached);
}

void tcp_session::write(std::string_view msg) {
    auto buff = sg::make_shared_c_buffer<std::byte>(msg.size());
    std::memcpy(buff.get(), msg.data(), msg.size());
    write(buff);
}
void tcp_session::write(const void* data, size_t size) {
    auto ptr = sg::make_shared_c_buffer<std::byte>(size);
    std::memcpy(ptr.get(), data, size);
    write(ptr);
}

void tcp_session::set_keepalive(keepalive_t keepAliveParameters) {
    sg::net::native::set_keepalive(m_socket.native_handle(), keepAliveParameters);
}

void tcp_session::set_timeout(unsigned timeoutMSec) {
    sg::net::native::set_timeout(m_socket.native_handle(), timeoutMSec);
}

native::socket_t tcp_session::native_handle() { return m_socket.native_handle(); }

void tcp_session::stop_async() {
    // No action if we have already stopped
    if (m_stopped.load(std::memory_order::acquire))
        return;

    m_stop_requested.store(true, std::memory_order::release);

    // spawn a writer to drain remaining messages then close,
    // or close immediately if nothing is pending
    bool need_spawn = false;
    {
        std::lock_guard lock(m_write_mutex);
        if (!m_write_msgs.empty())
            need_spawn = !std::exchange(m_write_scheduled, true);
    }

    if (need_spawn)
        co_spawn(m_socket.get_executor(), [this] { return writer(); }, boost::asio::detached);
    else
        close();
}

void tcp_session::wait_until_stopped() const {
    // block until m_stopped is true
    m_stopped.wait(false, std::memory_order::acquire);
}

bool tcp_session::is_connected() const {
    return !m_stopped.load(std::memory_order::acquire);
}

end_point tcp_session::local_endpoint() const noexcept(false) {
    // may throw on closed socket
    auto asioEp = m_socket.local_endpoint();
    return end_point(asioEp.address().to_string(), asioEp.port());
}

end_point tcp_session::remote_endpoint() const noexcept(false) {
    // may throw on closed socket
    auto asioEp = m_socket.remote_endpoint();
    return end_point(asioEp.address().to_string(), asioEp.port());
}

void tcp_session::close() {
    {
        // .close/.shutdown sock operations are not thread safe
        std::lock_guard lock(m_sock_op_mutex);

        /* graceful disconnection  */
        try {
            if (m_socket.is_open())
                m_socket.shutdown(m_socket.shutdown_both);
        } catch(...) {}

        /*  you still need to close the socket, even if the connection is down */
        try {
            m_socket.close();
        } catch (...) {}

        /* don't go past unless the reader has stopped */
        if (m_reader_running)
            return;

        if (m_disconnected_cb_called.exchange(true))
            return;
    }

    std::exception_ptr exPtr;
    {
        std::lock_guard lock(m_exception_mutex);
        exPtr =  m_exception;
    }

    if (m_on_disconnected_cb)
        m_on_disconnected_cb.invoke(*this, exPtr);

    m_stopped.store(true, std::memory_order::release);
    m_stopped.notify_all(); // needed for atomic wait()
}

boost::asio::awaitable<void> tcp_session::reader() {
    try {
        boost::asio::socket_base::receive_buffer_size option;
        m_socket.get_option(option);
        int size = option.value();

        auto data = std::make_unique<std::byte[]>(size);
        while (m_socket.is_open()) {
            if (m_options.dont_read) {
                co_await m_socket.async_wait(boost::asio::ip::tcp::socket::wait_read, boost::asio::use_awaitable);
                if (m_on_data_cb)
                    m_on_data_cb.invoke(*this, nullptr, 0);
            }
            else {
                std::size_t n = co_await m_socket.async_read_some(boost::asio::buffer(data.get(), size),
                                                              boost::asio::use_awaitable);
                if (m_on_data_cb)
                    m_on_data_cb.invoke(*this, data.get(), n);
            }
        }
    } catch (...) {
        /* if clean closing, do not throw error */
        if (!m_stop_requested.load(std::memory_order::acquire))
        {
            std::lock_guard lock(m_exception_mutex);
            if (!m_exception)
                m_exception = std::current_exception();
        }
    }

    m_reader_running.store(false);
    close();

    co_return;
}

boost::asio::awaitable<void> tcp_session::writer() {
    using namespace boost::asio::experimental::awaitable_operators;

    try {
        for (;;) {
            std::vector<sg::shared_c_buffer<std::byte>> buffers;
            {
                std::lock_guard lock(m_write_mutex);
                buffers.swap(m_write_msgs);
                if (buffers.empty()) {
                    m_write_scheduled = false;

                    if (m_stop_requested.load(std::memory_order::acquire))
                        close();

                    co_return;
                }
            }

            std::vector<boost::asio::const_buffer> buffersAsio;
            buffersAsio.reserve(buffers.size());
            for (auto& buff : buffers)
                buffersAsio.emplace_back(buff.get(), buff.size());

            // even though we set the socket time-out using SO_SNDTIMEO, it is not enforced for
            // select()/poll()/epoll_wait(), which might be used by asio internally
            auto deadline = std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(m_options.timeout_msec);

            auto result = co_await (
                boost::asio::async_write(m_socket, buffersAsio, boost::asio::use_awaitable) ||
                async_timeout(deadline));

            if (result.index() == 1)
                SG_THROW(exceptions::net<exceptions::errors::net::time_out>, "operation timeout");
        }
    } catch (...) {
        {
            std::lock_guard lock(m_exception_mutex);
            if (!m_exception)
                m_exception = std::current_exception();
        }
    }

    close();

    co_return;
}

boost::asio::awaitable<void>
tcp_session::async_timeout(const std::chrono::steady_clock::time_point& deadline)  {
    boost::asio::steady_timer timer(m_socket.get_executor());
    auto now = std::chrono::steady_clock::now();
    while (deadline > now) {
        timer.expires_at(deadline);
        co_await timer.async_wait(boost::asio::use_awaitable);
        now = std::chrono::steady_clock::now();
    }

}
} // namespace sg::net
