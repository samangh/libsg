#include "sg/tcp_session.h"
#include "sg/tcp_native.h"
#include "sg/debug.h"

#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace sg::net {

tcp_session::tcp_session(boost::asio::ip::tcp::socket socket, on_data_available_cb_t onReadCb,
                         on_disconnected_cb_t onErrorCb, options_t options)
    : m_socket(std::move(socket)),
      m_write_strand(boost::asio::make_strand(m_socket.get_executor())),
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
    m_stopped.store(false);

    try {
        if (onConn)
            onConn.invoke(*this);

        set_keepalive(m_options.keepalive);
        set_timeout(m_options.timeout_msec);

        m_reader_running.store(true, std::memory_order::release);
        co_spawn(
            m_socket.get_executor(), [self = this] { return self->reader(); },
            boost::asio::detached);
    } catch (...) {
        {
            std::lock_guard lock(m_exception_mutex);
            if (!m_exception)
                m_exception = std::current_exception();
        }

        close();
    }
}

void tcp_session::write(sg::shared_c_buffer<std::byte> msg) {
    if (m_stop_requested.load(std::memory_order::acquire))
        SG_THROW(std::runtime_error,
                 "attempt to write to tcp_session after a disconnection was requested");

    auto needtoScheduleWrite  = false;
    {
        std::lock_guard lock(m_write_mutex);
        m_write_msgs.push_back(std::move(msg));
        needtoScheduleWrite = !std::exchange(m_write_scheduled, true);
    }

    if (needtoScheduleWrite) {
        co_spawn(m_write_strand, [this] {
            return writer();
        }, boost::asio::detached);
    }
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
    close();
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

void tcp_session::close() {
    boost::asio::dispatch(m_write_strand, [this] {
        /* graceful disconnection  */
        try {
            if (m_socket.is_open())
                m_socket.shutdown(m_socket.shutdown_both);
        } catch (...) {}

        /*  you still need to close the socket, even if the connection is down */
        try {
            m_socket.close();
        } catch (...) {}

        if (m_reader_running.load(std::memory_order::acquire))
            return;

        std::exception_ptr exPtr;
        {
            std::lock_guard lock(m_exception_mutex);
            exPtr = m_exception;
        }

        if (m_on_disconnected_cb)
            m_on_disconnected_cb.invoke(*this, exPtr);

        m_stopped.store(true, std::memory_order::release);
        m_stopped.notify_all(); // needed for atomic wait()
    });
}

boost::asio::awaitable<void> tcp_session::reader() {
    try {
        boost::asio::socket_base::receive_buffer_size option;
        m_socket.get_option(option);
        int size = option.value();

        auto data = std::make_unique<std::byte[]>(size);
        if (m_options.dont_read) {
            co_await m_socket.async_wait(boost::asio::ip::tcp::socket::wait_read,
                                         boost::asio::use_awaitable);
            if (m_on_data_cb)
                m_on_data_cb.invoke(*this, nullptr, 0);
        } else {
            std::size_t n = co_await m_socket.async_read_some(boost::asio::buffer(data.get(), size),
                                                              boost::asio::use_awaitable);
            if (m_on_data_cb)
                m_on_data_cb.invoke(*this, data.get(), n);
        }

        co_spawn(m_socket.get_executor(), [this] { return reader(); }, boost::asio::detached);
    } catch (...) {
        /* if clean closing, do not throw error */
        if (!m_stop_requested.load(std::memory_order::acquire))
        {
            std::lock_guard lock(m_exception_mutex);
            if (!m_exception)
                m_exception = std::current_exception();
        }

        //TODO: I don't like this, better to dispatch another lambda to asio context to check this on regular basis!
        m_reader_running.store(false, std::memory_order::release);
        close();
    }
}

boost::asio::awaitable<void> tcp_session::writer() {
    using namespace boost::asio::experimental::awaitable_operators;

    /* swap data */
    std::vector<shared_c_buffer<std::byte>> buff{};
    {
        std::lock_guard lock(m_write_mutex);
        buff.swap(m_write_msgs);
        m_write_scheduled = false;
    }

    try {
        /* create ASIO buffers */
        std::vector<boost::asio::const_buffer> asioBuffers;
        for (const auto& buffer : buff) asioBuffers.emplace_back(buffer.get(), buffer.size());

            // even though we set the socket time-out using SO_SNDTIMEO, it is not enforced for
            // select()/poll()/epoll_wait(), which might be used by asio internally
            auto deadline = std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(m_options.timeout_msec);

        const auto result =
            co_await (boost::asio::async_write(m_socket, asioBuffers, boost::asio::use_awaitable) ||
                      async_timeout(deadline));

            if (result.index() == 1)
                SG_THROW(exceptions::net<exceptions::errors::net::time_out>, "operation timeout");

    } catch (...) {
        {
            std::lock_guard lock(m_exception_mutex);
            if (!m_exception)
                m_exception = std::current_exception();
        }
        close();
    }
}

boost::asio::awaitable<void>
tcp_session::async_timeout(std::chrono::steady_clock::time_point& deadline)  {
    boost::asio::steady_timer timer(m_socket.get_executor());
    auto now = std::chrono::steady_clock::now();
    while (deadline > now) {
        timer.expires_at(deadline);
        co_await timer.async_wait(boost::asio::use_awaitable);
        now = std::chrono::steady_clock::now();
    }

}
} // namespace sg::net
