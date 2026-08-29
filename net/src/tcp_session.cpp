#include "sg/net/tcp_session.h"
#include "sg/net/tcp_native.h"
#include "sg/debug.h"

#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/co_spawn.hpp>

#include <fmt/format.h>

#include <iostream>
#include <stdexcept>

namespace sg::net {
namespace {

/* The session is still up -- carrying data, or negotiating its way there -- as opposed to already
 * tearing down. This is the window in which the cause of a teardown is worth recording. */
constexpr bool is_alive(tcp_session::state_t state) {
    return state == tcp_session::state_t::running || state == tcp_session::state_t::handshaking;
}

} // namespace

std::shared_ptr<tcp_session> tcp_session::create(boost::asio::io_context& context,
                                                 boost::asio::ip::tcp::socket socket,
                                                 transport_factory factory,
                                                 Callbacks callbacks, options_t options) {

    return std::make_shared<tcp_session>(private_tag{}, context, std::move(socket),
                                         std::move(factory), std::move(callbacks), options);
}

tcp_session::tcp_session(private_tag, boost::asio::io_context& context,
                         boost::asio::ip::tcp::socket socket,
                         transport_factory factory, Callbacks cb, options_t options)
: m_socket(std::move(socket)),
  /* Built here, now that m_socket is settled into place, so the transport can bind to it for life
   * rather than being handed it on every call. */
  m_transport(make_transport(factory, m_socket)),
  m_strand(boost::asio::make_strand(m_socket.get_executor())),
  m_io_executor(context.get_executor()),
  m_options(options),
  m_callbacks(std::move(cb)) {
    // the caller must hand us the context the socket was created on
    assert(m_socket.get_executor() == boost::asio::any_io_executor(m_io_executor));

    if (m_options.dont_read && !m_callbacks.onDataAvailable)
        SG_THROW(std::invalid_argument, "tcp_session: options_t::dont_read requires an "
                                        "OnDataAvailable callback to read the socket");

    if (m_options.dont_read && m_transport->is_negotiated())
        SG_THROW(std::invalid_argument,
                 "tcp_session: options_t::dont_read cannot be combined with a negotiated "
                 "transport; only the transport can decode what is on the socket");
};

/* you can be sure that by the time this is called all the callbacks are done:
 *
 *   - OnDisconnected/OnDisconnected callbacks are on the strand, which has a shared_ptr to this class
 *   - OnConnected is done on start()
 */
tcp_session::~tcp_session() {
    assert(m_state == state_t::stopped);
}

void tcp_session::start() {
    if (m_start_called.test_and_set(std::memory_order::acq_rel))
        SG_THROW(std::logic_error,
                 "tcp_session::start() has already been called; a session cannot be restarted");

    /* A negotiated transport is not usable until it has handshaked, but a plain one can be used
     * immediately.
     *
     * For plain transport, start in running immediately. This is because some old clients assume
     * you can write() to the session as soon as OnConnected() is called */
    const bool negotiated   = m_transport->is_negotiated();
    const auto initialState = negotiated ? state_t::handshaking : state_t::running;

    /* start in handshake state (even if the transport does not need it, for teh sake of uniformity */
    if (auto expectedState = state_t::stopped; !m_state.compare_exchange_strong(
            expectedState, initialState, std::memory_order::acq_rel, std::memory_order::acquire))
        SG_THROW(std::runtime_error, "tcp_session is already running");

    try {
        // note: below can throw if the client has disconnected.
        // Safe to call the *_unsafe helpers directly here: the reader/writer coroutines
        // haven't been spawned yet, so we're the only thread touching the socket.
        apply_keepalive_unsafe(m_options.keepalive);
        apply_timeout_unsafe(m_options.timeout_msec);

        // store local/remote endpoints, so that we don't have to faff about with strands later, as
        // socket access needs to be through a strand due to thread safety
        auto remEP = m_socket.remote_endpoint();
        auto localEP = m_socket.local_endpoint();
        m_remote_endpoint = end_point(remEP.address().to_string(), remEP.port());
        m_local_endpoint = end_point(localEP.address().to_string(), localEP.port());

        if (m_options.recv_buffer_size)
            sg::net::native::set_recv_buffer_size(m_socket.native_handle(), m_options.recv_buffer_size);
        if (m_options.send_buffer_size)
            sg::net::native::set_send_buffer_size(m_socket.native_handle(), m_options.send_buffer_size);

        /* Reported connected as soon as the raw session is set up, before any negotiation, and on
         * both paths.
         *
         * This is what callers hang their bookkeeping on -- tcp_server registers the session here
         * -- and an unregistered session is one nothing can find, stop or wait for. Leaving a
         * negotiating session unregistered meant tcp_server's shutdown drain could not see it and
         * stopped the io_context underneath it, taking the session with it mid-handshake.
         *
         * The debt is taken on at the same moment, so OnConnected and OnDisconnected are always a
         * pair: a handshake that fails is now reported as a disconnection with that error, rather
         * than being dropped. Anything counting one against the other stays balanced. */
        m_disconnection_callback_owed.store(true, std::memory_order::release);
        if (m_callbacks.onConnected)
            m_callbacks.onConnected.invoke(*this);

        spawn(negotiated ? &tcp_session::negotiate_and_read : &tcp_session::reader);
    } catch (...) {
        /* if clean closing, do not throw error */
        record_error();

        stop_async();

        /* Never block a pool worker: tcp_server::listener() calls start() on one. stop_async() has
         * already made the teardown unstoppable, and close() holds a shared_ptr to us, so the
         * session still reaches `stopped` on its own. */
        if (!running_in_io_thread())
            wait_until_stopped();

        throw;
    }
}

boost::asio::awaitable<void> tcp_session::negotiate_and_read() {
    try {
        co_await m_transport->handshake();

        /* Handshake done, so the session is `running` and writable at last. Nothing was buffered
         * while it negotiated (write() refused it), so there is no writer to start -- just flip the
         * state. A CAS, not a store: a close() may have landed mid-handshake and must not be undone. */
        if (auto expected = state_t::handshaking;
            !m_state.compare_exchange_strong(expected, state_t::running,
                                             std::memory_order::acq_rel,
                                             std::memory_order::acquire))
            co_return; /* stopped underneath us; close() already owns the teardown */

        m_state.notify_all();
    } catch (...) {
        record_error();
        close();
        co_return;
    }

    co_await reader();
}

boost::asio::awaitable<void> tcp_session::close_gracefully() {
    /* Best effort by contract: shutdown() does not throw, because close_impl() has to run. */
    co_await m_transport->shutdown();

    close_impl();
}

void tcp_session::write(sg::shared_c_buffer<std::byte> msg) {
    bool need_spawn = false;
    {
        std::lock_guard lock(m_write_mutex);

        /* Only a `running` session can take data: a still-negotiating one has no transport able to
         * carry it yet, and everything else is on its way down. */
        if (m_state.load(std::memory_order::acquire) != state_t::running)
            SG_THROW(std::runtime_error, "attempt to write to a non-operational tcp_session");

        /* Tested before this message is counted, so that a message bigger than the mark is never
         * unsendable. */
        if (auto mark = m_options.write_high_water_mark; mark && pending_bytes() >= mark)
            SG_THROW(exceptions::net::buffer_full,
                     fmt::format("tcp_session: {} bytes are already pending, at or over the "
                                 "high-water mark of {} bytes",
                                 pending_bytes(), mark));

        m_queued_bytes.fetch_add(msg.size(), std::memory_order::relaxed);
        m_write_msgs.push_back(std::move(msg));
        need_spawn = !std::exchange(m_write_scheduled, true);
    }

    //co_spawn might be slow, so have it outside the lock
    if (need_spawn)
        spawn(&tcp_session::writer);
}

void tcp_session::write(std::string_view msg) {
    auto buff = sg::make_shared_c_buffer<std::byte>(msg.size());
    std::memcpy(buff.get(), msg.data(), msg.size());
    write(std::move(buff));
}
void tcp_session::write(const void* data, size_t size) {
    auto ptr = sg::make_shared_c_buffer<std::byte>(size);
    std::memcpy(ptr.get(), data, size);
    write(std::move(ptr));
}

void tcp_session::apply_keepalive_unsafe(keepalive_t keepAliveParameters) {
    sg::net::native::set_keepalive(m_socket.native_handle(), keepAliveParameters);
    m_options.keepalive = keepAliveParameters;
}

void tcp_session::apply_timeout_unsafe(unsigned timeoutMSec) {
    /* this has no effect in async read/write, but we apply it anyway just in case the user uses
     * the native_handle */
    sg::net::native::set_timeout(m_socket.native_handle(), timeoutMSec);

    // m_options.timeout_msec is read by writer() under m_write_mutex (see writer() for the read).
    std::lock_guard lock(m_write_mutex);
    m_options.timeout_msec = timeoutMSec;
}

/* The socket is not thread-safe, so any setsockopt() on its native handle must be serialised with
 * reader()/writer()/close_impl(). Route the work through m_strand via a std::packaged_task so
 * exceptions thrown by are captured and rethrown in the caller's thread.
 *
 * boost::asio::dispatch runs the task inline if the caller is already on m_strand (e.g. invoked
 * from on_data_cb), so fut.get() will return immediately in that case rather than deadlocking. */
void tcp_session::set_keepalive(keepalive_t keepAliveParameters) {
    run_in_executor([this, keepAliveParameters]() { apply_keepalive_unsafe(keepAliveParameters); });
}

void tcp_session::set_timeout(unsigned timeoutMSec) {
    run_in_executor([this, timeoutMSec]() { apply_timeout_unsafe(timeoutMSec); });
}

size_t tcp_session::pending_bytes() const noexcept {
    return m_queued_bytes.load(std::memory_order::relaxed) +
           m_writing_bytes.load(std::memory_order::relaxed);
}

enum tcp_session::state_t tcp_session::state() const noexcept {
    return m_state.load(std::memory_order::acquire);
}

native::socket_t tcp_session::native_handle() { return m_socket.native_handle(); }
boost::asio::any_io_executor tcp_session::get_executor() const {return m_strand;}

bool tcp_session::running_in_io_thread() const {
    return m_io_executor.running_in_this_thread();
}

void tcp_session::stop_async() {
    bool shouldClose = false;

    {
        // make sure that stop_async runs after all writes are scheduled
        std::lock_guard lock(m_write_mutex);

        // Only act if currently running — CAS handles both "already stopped" and "already requested"
        auto expected = state_t::running;
        if (!m_state.compare_exchange_strong(expected, state_t::stop_requested,
                                             std::memory_order::acq_rel,
                                             std::memory_order::acquire)) {
            /* Still negotiating: no writer and nothing buffered (write() refuses data until it is
             * up), so nothing to drain -- close now. Any other state is already going down. */
            if (expected != state_t::handshaking)
                return;

            shouldClose = true;
        }
        else
            // if a writer is running, it will close connection due to state_stop_requested
            shouldClose = !m_write_scheduled;
    }

    /* We make sure that that the close() is called when the lock is not held. This is because if
     * stop_async() is called from on_data_cb (which runs on the strand), the dispatched close_impl
     * runs inline, and invokes on_disconnected_cb. If that callback accidentally calls write()
     * then you have a deadlock */
    if (shouldClose)
        close(true);
}

void tcp_session::stop_async_force() {
    /* Closes now if the session is still up, skipping any graceful sign-off. */
    close(false);

    /* If a graceful stop got to the transition first, the call above was a no-op and
     * close_gracefully() is parked in the transport's shutdown() (up to shutdown_timeout_msec, or
     * forever if 0). Close the socket out from under it so the shutdown fails at once and
     * close_gracefully() falls through to close_impl() -- this escalates rather than waits. On the
     * strand (the socket is not thread-safe); the double close with close_impl() is harmless. */
    if (m_state.load(std::memory_order::acquire) == state_t::stopping)
        boost::asio::dispatch(m_strand, [self = shared_from_this()] {
            boost::system::error_code ec;
            self->m_socket.close(ec);
        });
}

void tcp_session::wait_until_stopped() const {
    wait_until("wait_until_stopped", [](state_t s) { return s == state_t::stopped; });
}

void tcp_session::wait_until_ready() const {
    wait_until("wait_until_ready",
               [](state_t s) { return s == state_t::stopped || s == state_t::running; });
}

bool tcp_session::is_connected() const noexcept {
    /* `handshaking` does NOT count: the session has been reported connected so callers can find
     * it, but it cannot carry data and refuses writes until the handshake lands. */
    return m_state.load(std::memory_order::acquire) == state_t::running;
}

bool tcp_session::is_negotiated() const noexcept { return m_transport->is_negotiated(); }

std::exception_ptr tcp_session::last_error() const {
    std::lock_guard lock(m_exception_mutex);
    return m_exception;
}

end_point tcp_session::local_endpoint() const {
    return m_local_endpoint;
}

end_point tcp_session::remote_endpoint() const {
    return m_remote_endpoint;
}

void tcp_session::record_error() {
    if (!is_alive(m_state.load(std::memory_order::acquire)))
        return;

    std::lock_guard lock(m_exception_mutex);
    if (!m_exception)
        m_exception = std::current_exception();
}

void tcp_session::spawn(boost::asio::awaitable<void> (tcp_session::*coro)()) {
    co_spawn(m_strand, [self = shared_from_this(), coro] { return (self.get()->*coro)(); },
             boost::asio::detached);
}

void tcp_session::wait_until(const char* caller, bool (*done)(state_t)) const {
    /* All session handlers -- callbacks, reader/writer, close_impl -- run on m_strand, so waiting
     * from inside one would block the very handler being waited for. */
    if (running_in_io_thread())
        SG_THROW(std::logic_error,
                 fmt::format("tcp_session::{}() must not be called from a handler running on the "
                             "I/O pool (e.g. a session callback); it would block the handler it "
                             "is waiting for",
                             caller));

    state_t val;
    while (!done(val = m_state.load(std::memory_order::acquire)))
        m_state.wait(val, std::memory_order::acquire);
}

void tcp_session::close(bool graceful) {
    // Transition to closing. Only one caller succeeds.
    auto cur = m_state.load(std::memory_order::acquire);
    while (cur == state_t::running || cur == state_t::stop_requested ||
           cur == state_t::handshaking) {
        if (m_state.compare_exchange_weak(cur, state_t::stopping,
                                           std::memory_order::acq_rel,
                                           std::memory_order::acquire))
        {
            /* so that wait_until_ready() does not have to sleep until close_impl() gets there */
            m_state.notify_all();

            // Socket operations must be serialised with the reader/writer, so run close_impl()
            // on the session strand (same strand the reader and writer run on).

            if (graceful && cur != state_t::handshaking)
                spawn(&tcp_session::close_gracefully);
            else
                boost::asio::dispatch(m_strand, [self = shared_from_this()] { self->close_impl(); });
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

    try {
        if (m_disconnection_callback_owed.load(std::memory_order::acquire))
            if (m_callbacks.onDisconnected)
                m_callbacks.onDisconnected.invoke(*this, exPtr);
    } catch (const std::exception& ex) {
        std::cerr << "tcp_session: onDisconnected() callback exception caught: " << ex.what()
                  << std::endl;
    } catch (...) {
        std::cerr << "tcp_session: onDisconnected() callback exception caught" <<  std::endl;
    }

    m_state.store(state_t::stopped, std::memory_order::release);
    m_state.notify_all();
}

boost::asio::awaitable<void> tcp_session::reader() {
    /* reader() and writer() share m_strand, but that does NOT serialise their I/O. A strand
     * gates handler *execution* (one handler on a CPU at a time), not operation *pendency*:
     * it is released the moment a coroutine hits co_await. So while a write is pending the
     * writer is suspended and off the strand, leaving us free to run here and start a read.
     * A read and a write are therefore in flight simultaneously (full duplex); only the brief
     * completion-handler bodies are serialised, never the network I/O. */
    try {
        boost::asio::socket_base::receive_buffer_size option;
        m_socket.get_option(option);
        int size = option.value();

        auto data = std::make_unique<std::byte[]>(size);
        while (m_socket.is_open()) {
            if (m_options.dont_read) {
                /* m_socket.is_open() only checks whether *we* have closed our end. If the peer
                 * has disconnected, it will remain true.
                 *
                 * Normally, what happens is that we run async_read_some(..), which then throws
                 * boost::asio::error::eof exception and causes us to eventually call close().
                 *
                 * A one-byte MSG_PEEK receive waits for readability. It leaves the byte in the
                 * kernel buffer for the callback to read, and throws boost::asio::error::eof if
                 * peer has closed, exactly as the async_read_some() in the normal path below does.
                 */
                std::byte peeked;
                co_await m_socket.async_receive(boost::asio::buffer(&peeked, 1),
                                                boost::asio::socket_base::message_peek,
                                                boost::asio::use_awaitable);

                // m_socket.available() could be used to get number of bytes available
                m_callbacks.onDataAvailable.invoke(*this, nullptr, 0);
            }
            else {
                std::size_t n =
                    co_await m_transport->read_some(boost::asio::buffer(data.get(), size));
                if (m_callbacks.onDataAvailable)
                    m_callbacks.onDataAvailable.invoke(*this, data.get(), n);
            }
        }
    } catch (...) {
        /* if clean closing, do not throw error
         *
         * We have to do this because during graceful shutdown, close() will close the socket and so
         * cause the reader to throw.
         */
        record_error();
    }

    close();
}

boost::asio::awaitable<void> tcp_session::writer() {
    try {
        for (;;) {
            unsigned timeoutMSec;
            std::vector<sg::shared_c_buffer<std::byte>> buffers;
            {
                std::lock_guard lock(m_write_mutex);
                buffers.swap(m_write_msgs);
                if (buffers.empty())
                    m_write_scheduled = false;

                timeoutMSec = m_options.timeout_msec;

                /* Update byte count */
                size_t batch = 0;
                for (const auto& buff : buffers)
                    batch += buff.size();
                m_queued_bytes.fetch_sub(batch, std::memory_order::relaxed);
                m_writing_bytes.store(batch, std::memory_order::relaxed);
            }

            // make sure close() is called outside the m_write_mutex lock, as close() could call
            // the user-defined on-disconnection callback. If in the callback the user calls
            // write(), we'll have a deadlock as write() as locks that mutex
            if (buffers.empty()) {
                if (m_state.load(std::memory_order::acquire)!= state_t::running)
                    close(true);
                co_return;
            }

            std::vector<boost::asio::const_buffer> buffersAsio;
            buffersAsio.reserve(buffers.size());
            for (auto& buff : buffers)
                buffersAsio.emplace_back(buff.get(), buff.size());

            if (auto ec = co_await m_transport->write_all(buffersAsio, timeoutMSec); ec) {
                /* Only report a timeout if we are meant to be running, as a normal close cancels
                 * the write and also surfaces as operation_aborted. */
                if (timeoutMSec && m_state.load(std::memory_order::acquire) == state_t::running)
                    if (ec == boost::asio::error::timed_out ||
                        ec == boost::asio::error::operation_aborted)
                        SG_THROW(exceptions::net::time_out);
                throw boost::system::system_error(ec);
            }
        }
    } catch (...) {
        // We should end up here during graceful shutdown, as the writer waits until all messages
        // are written
        record_error();
    }

    close();
}
} // namespace sg::net
