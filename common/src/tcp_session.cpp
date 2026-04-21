#include "sg/tcp_session.h"
#include "sg/tcp_native.h"
#include "sg/debug.h"

#include <boost/asio/detached.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace sg::net {

std::shared_ptr<tcp_session> tcp_session::create(boost::asio::ip::tcp::socket socket,
                                                  on_data_available_cb_t onReadCb,
                                                  on_disconnected_cb_t onErrorCb,
                                                  options_t options) {
    return std::make_shared<tcp_session>(private_tag{}, std::move(socket),
                                         std::move(onReadCb), std::move(onErrorCb),
                                         options);
}

tcp_session::tcp_session(private_tag, boost::asio::ip::tcp::socket socket,
                         on_data_available_cb_t onReadCb,
                         on_disconnected_cb_t onErrorCb, options_t options)
    : m_socket(std::move(socket)),
      m_options(options),
      m_on_data_cb(std::move(onReadCb)),
      m_on_disconnected_cb(std::move(onErrorCb)) {}

tcp_session::~tcp_session() {
    // You can't use shared_from_this() in the destructor, so this flag tells close() not to
    // dispatch the close even to io_context but to run it directly
    m_destructor_called = true;

    // this causes the right callbacks to be called, if they haven't already
    stop_async();
    wait_until_stopped();
}

void tcp_session::start(on_connected_cb_t onConn) {
    try {
        m_state.store(state_t::running);

        if (onConn)
            onConn.invoke(*this);

        // note: below can throw if the client has disconnected
        set_keepalive(m_options.keepalive);
        set_timeout(m_options.timeout_msec);
        if (m_options.recv_buffer_size)
            sg::net::native::set_recv_buffer_size(m_socket.native_handle(), m_options.recv_buffer_size);
        if (m_options.send_buffer_size)
            sg::net::native::set_send_buffer_size(m_socket.native_handle(), m_options.send_buffer_size);

        co_spawn(m_socket.get_executor(), [self = shared_from_this()] { return self->reader(); }, boost::asio::detached);
    } catch (...) {
        /* if clean closing, do not throw error */
        if (m_state.load(std::memory_order::acquire) == state_t::running)
        {
            std::lock_guard lock(m_exception_mutex);
            if (!m_exception)
                m_exception = std::current_exception();
        }

        stop_async();
        wait_until_stopped();
    }
}

void tcp_session::write(sg::shared_c_buffer<std::byte> msg) {
    bool need_spawn = false;
    {
        std::lock_guard lock(m_write_mutex);

        if (m_state.load(std::memory_order::acquire) != state_t::running)
            SG_THROW(std::runtime_error,
                     "attempt to write to tcp_session after a disconnection was requested");


        m_write_msgs.push_back(std::move(msg));
        need_spawn = !std::exchange(m_write_scheduled, true);
    }

    //co_spawin might be slow, so have it outside the lock
    if (need_spawn)
        co_spawn(m_socket.get_executor(), [self = shared_from_this()] { return self->writer(); }, boost::asio::detached);
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

    // nothing actually reads back the keepalive parameters - no need for locks
    m_options.keepalive = keepAliveParameters;
}

void tcp_session::set_timeout(unsigned timeoutMSec) {
    sg::net::native::set_timeout(m_socket.native_handle(), timeoutMSec);

    // timeout is used by writer, so make sure it's thread safe
    std::lock_guard lock(m_write_mutex);
    m_options.timeout_msec = timeoutMSec;
}
enum tcp_session::state_t tcp_session::state() const noexcept {
    return m_state.load(std::memory_order::acquire);
}

native::socket_t tcp_session::native_handle() { return m_socket.native_handle(); }

void tcp_session::stop_async() {
    // make sure that stop_async runs after all writes are scheduled
    std::lock_guard lock(m_write_mutex);

    // Only act if currently running — CAS handles both "already stopped" and "already requested"
    auto expected = state_t::running;
    if (!m_state.compare_exchange_strong(expected, state_t::stop_requested,
                                         std::memory_order::release,
                                         std::memory_order::acquire))
        return;

    // if a writer is running, it will close connection due to state_stop_requested
    if (!m_write_scheduled)
        close();
}

void tcp_session::wait_until_stopped() const {
    state_t val;
    while ((val = m_state.load(std::memory_order::acquire)) != state_t::stopped)
        m_state.wait(val, std::memory_order::acquire);
}

bool tcp_session::is_connected() const noexcept {
    return m_state.load(std::memory_order::acquire) != state_t::stopped;
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
    // Transition to closing (from running or stop_requested). Only one caller succeeds.
    auto cur = m_state.load(std::memory_order::acquire);
    while (cur == state_t::running || cur == state_t::stop_requested) {
        if (m_state.compare_exchange_weak(cur, state_t::stopping,
                                           std::memory_order::release,
                                           std::memory_order::acquire))
        {
            // socket operations should be run on the io_context, as they are technically thread safe.
            //
            // You can't use shared_from_this() in the destructor, so if we are in the destructor run
            // close_impl() directly. In this case thread-safety is not important (as by definition no other
            // thread can be holding a shared_ptr to this session!)
            if (m_destructor_called)
                close_impl();
            else
                boost::asio::dispatch(m_socket.get_executor(),
                                      [self = shared_from_this()] { self->close_impl(); });
            return;
        }
    }
    // already closing or stopped — nothing to do
}
void tcp_session::close_impl() {
    /* graceful disconnection  */
    try {
        if (m_socket.is_open())
            m_socket.shutdown(m_socket.shutdown_both);
    } catch (...) {}

    /*  you still need to close the socket, even if the connection is down */
    try {
        m_socket.close();
    } catch (...) {}

    std::exception_ptr exPtr;
    {
        std::lock_guard lock(m_exception_mutex);
        exPtr =  m_exception;
    }

    if (m_on_disconnected_cb)
        m_on_disconnected_cb.invoke(*this, exPtr);

    m_state.store(state_t::stopped, std::memory_order::release);
    m_state.notify_all();
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
        if (m_state.load(std::memory_order::acquire) == state_t::running)
        {
            std::lock_guard lock(m_exception_mutex);
            if (!m_exception)
                m_exception = std::current_exception();
        }
    }

    close();
}

boost::asio::awaitable<void> tcp_session::writer() {
    using namespace boost::asio::experimental::awaitable_operators;

    try {
        for (;;) {
            unsigned timeoutMSec;
            std::vector<sg::shared_c_buffer<std::byte>> buffers;
            {
                std::lock_guard lock(m_write_mutex);
                buffers.swap(m_write_msgs);
                if (buffers.empty()) {
                    m_write_scheduled = false;

                    if (m_state.load(std::memory_order::acquire)!= state_t::running)
                        close();

                    co_return;
                }

                timeoutMSec = m_options.timeout_msec;
            }

            std::vector<boost::asio::const_buffer> buffersAsio;
            buffersAsio.reserve(buffers.size());
            for (auto& buff : buffers)
                buffersAsio.emplace_back(buff.get(), buff.size());

            // even though we set the socket time-out using SO_SNDTIMEO, it is not enforced for
            // select()/poll()/epoll_wait(), which might be used by asio internally
            auto deadline = std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(timeoutMSec);

            auto result = co_await (
                boost::asio::async_write(m_socket, buffersAsio, boost::asio::use_awaitable) ||
                async_timeout(deadline));

            if (result.index() == 1)
                SG_THROW(exceptions::net<exceptions::errors::net::time_out>, "operation timeout");
        }
    } catch (...) {
        // We should end up here during graceful shutdown, as the writer waits until all messages
        // are written
        {
            std::lock_guard lock(m_exception_mutex);
            if (!m_exception)
                m_exception = std::current_exception();
        }
    }

    close();
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
