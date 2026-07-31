#include "sg/net/tcp_session.h"
#include "sg/net/tcp_native.h"
#include "sg/debug.h"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/co_spawn.hpp>

namespace sg::net {

std::shared_ptr<tcp_session> tcp_session::create(boost::asio::ip::tcp::socket socket,
                                                 Callbacks callbacks, options_t options) {

    return std::make_shared<tcp_session>(private_tag{}, std::move(socket), std::move(callbacks),
                                         options);
}

tcp_session::tcp_session(private_tag, boost::asio::ip::tcp::socket socket, Callbacks cb,
                         options_t options)
: m_socket(std::move(socket)),
  m_strand(boost::asio::make_strand(m_socket.get_executor())),
  m_io_work(m_socket.get_executor()),
  m_options(options),
  m_callbacks(std::move(cb)),
  m_read_gate(m_strand) {
    /* The gate never expires on its own: the only things that complete a wait on it are
     * release_read_gate()'s cancel() and the destruction of the io_context. */
    m_read_gate.expires_at(boost::asio::steady_timer::time_point::max());
};

tcp_session::read_lease::~read_lease() { release(); }

tcp_session::read_lease& tcp_session::read_lease::operator=(read_lease&& other) noexcept {
    if (this != &other) {
        release();
        m_session = std::move(other.m_session);
    }
    return *this;
}

void tcp_session::read_lease::release() noexcept {
    auto session = std::move(m_session);
    if (!session)
        return;

    /* Publish the latch first. It is what the reader tests, so a reader that has not parked yet is
     * already free to continue and the wake-up below is pure nudge. */
    session->m_read_released.store(true, std::memory_order::release);

    /* Once the session has stopped there is nothing left to wake: close_impl() released the gate on
     * its way out, and reader() will not park again on a closed socket. Bail out before touching the
     * strand, because a lease dropped this late may well have outlived the io_context the strand
     * belongs to -- holding a lease keeps the *session* alive, not the pool it runs on.
     *
     * This is why reader() must never park while the socket is closed: it would be parking on a gate
     * that this branch has already given up on opening. */
    if (session->m_state.load(std::memory_order::acquire) == state_t::stopped)
        return;

    /* The timer is not thread-safe, so the cancel has to happen on the session's strand:
     * dispatch(), not post(), so that a callback which simply returns -- and is therefore already
     * on that strand -- runs it inline and the reader never suspends at all.
     *
     * The handler holds a weak_ptr, deliberately never a strong one, and this function drops its
     * own strong reference the moment it returns. There is nothing left for it to keep alive: the
     * session is still running (we checked), so m_io_work is still held and the io_context cannot
     * stop before this dispatch has run -- and if the reader has already gone, waking it is moot. */
    try {
        boost::asio::dispatch(session->m_strand, [weak = std::weak_ptr<tcp_session>(session)]() {
            if (auto s = weak.lock())
                s->m_read_gate.cancel();
        });
    } catch (...) {
        /* dispatch() only fails on allocation failure. The latch is already set, so a reader that
         * has not parked continues regardless; one that has parked waits until close(). */
    }
}

/* you can be sure that by the time this is called all the callbacks are done:
 *
 *   - OnDisconnected/OnDisconnected callbacks are on the strand, which has a shared_ptr to this class
 *   - OnConnected is done on start()
 */
tcp_session::~tcp_session() {
    assert(m_state == state_t::stopped);
}

void tcp_session::start() {
    if (auto expectedState = state_t::stopped; !m_state.compare_exchange_strong(
            expectedState, state_t::running, std::memory_order::acq_rel, std::memory_order::acquire))
        SG_THROW(std::runtime_error, "tcp_session is already running");

    try {
        // note: below can throw if the client has disconnected.
        // Safe to call the *_unsafe helpers directly here: the reader/writer coroutines
        // haven't been spawned yet, so we're the only thread touching m_socket.
        apply_keepalive_unsafe(m_options.keepalive);
        apply_timeout_unsafe(m_options.timeout_msec);

        // store local/remote endpoints, so that we don't have to faff about with strands later, as
        // m_socket access needs to be through a strand due to thread safety
        auto remEP = m_socket.remote_endpoint();
        auto localEP = m_socket.local_endpoint();
        m_remote_endpoint = end_point(remEP.address().to_string(), remEP.port());
        m_local_endpoint = end_point(localEP.address().to_string(), localEP.port());

        if (m_options.recv_buffer_size)
            sg::net::native::set_recv_buffer_size(m_socket.native_handle(), m_options.recv_buffer_size);
        if (m_options.send_buffer_size)
            sg::net::native::set_send_buffer_size(m_socket.native_handle(), m_options.send_buffer_size);

        if (m_callbacks.onConnected)
            m_callbacks.onConnected.invoke(*this);

        co_spawn(m_strand, [self = shared_from_this()] { return self->reader(); }, boost::asio::detached);
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

        throw;
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
        co_spawn(m_strand, [self = shared_from_this()] { return self->writer(); }, boost::asio::detached);
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

/* m_socket is not thread-safe, so any setsockopt() on its native handle must be serialised with
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
enum tcp_session::state_t tcp_session::state() const noexcept {
    return m_state.load(std::memory_order::acquire);
}

native::socket_t tcp_session::native_handle() { return m_socket.native_handle(); }
boost::asio::any_io_executor tcp_session::get_executor() const {return m_strand;}

bool tcp_session::running_in_io_thread() const {
    return m_strand.running_in_this_thread();
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
                                             std::memory_order::acquire))
            return;

        // if a writer is running, it will close connection due to state_stop_requested
        shouldClose= !m_write_scheduled;
    }

    /* We make sure that that the close() is called when the lock is not held. This is because if
     * stop_async() is called from on_data_cb (which runs on the strand), the dispatched close_impl
     * runs inline, and invokes on_disconnected_cb. If that callback accidentally calls write()
     * then you have a deadlock */
    if (shouldClose)
        close();
}

void tcp_session::wait_until_stopped() const {
    /* all session handlers (callbacks, reader/writer, close_impl) run on m_strand, so waiting
     * here from inside a strand handler would block close_impl from ever running */
    if (running_in_io_thread())
        SG_THROW(std::logic_error,
                 "tcp_session::wait_until_stopped() must not be called from a handler running on "
                 "the I/O pool (e.g. a session callback); use stop_async() instead");

    state_t val;
    while ((val = m_state.load(std::memory_order::acquire)) != state_t::stopped)
        m_state.wait(val, std::memory_order::acquire);
}

bool tcp_session::is_connected() const noexcept {
    return m_state.load(std::memory_order::acquire) != state_t::stopped;
}

end_point tcp_session::local_endpoint() const {
    return m_local_endpoint;
}

end_point tcp_session::remote_endpoint() const {
    return m_remote_endpoint;
}

void tcp_session::close() {
    // Transition to closing (from running or stop_requested). Only one caller succeeds.
    auto cur = m_state.load(std::memory_order::acquire);
    while (cur == state_t::running || cur == state_t::stop_requested) {
        if (m_state.compare_exchange_weak(cur, state_t::stopping,
                                           std::memory_order::acq_rel,
                                           std::memory_order::acquire))
        {
            // Socket operations must be serialised with the reader/writer, so run close_impl()
            // on the session strand (same strand the reader and writer run on).

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

    /* A reader parked on the gate has to be woken here, exactly as the close above wakes one
     * parked in async_read_some. Without this, a lease that is never released -- or one held by a
     * handler that is discarded rather than run -- would strand the reader coroutine, and with it
     * the shared_ptr it holds to us, for as long as the io_context lives. */
    release_read_gate();

    std::exception_ptr exPtr;
    {
        std::lock_guard lock(m_exception_mutex);
        exPtr =  m_exception;
    }

    if (m_callbacks.onDisconnected)
        m_callbacks.onDisconnected.invoke(*this, exPtr);

    /* Nothing more will be posted to m_strand on our behalf from here: write() refuses a session
     * that is not running, close() is a no-op once we have been through it, and a lease released
     * from now on returns without touching the strand. So the io_context is free to stop, and it
     * must be -- a tcp_server's pool has no work guard of its own and this is what lets it drain.
     *
     * Whatever is already queued on the strand (the read the close above cancelled, a reader woken
     * off the gate) keeps the context up on its own account until it has run.
     *
     * Before the state store, so that a wait_until_stopped() returning cannot be racing us for the
     * io_context we are about to let go of. */
    m_io_work.reset();

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
        auto size = option.value();

        m_read_buffer = sg::unique_buffer(new std::byte[size], size);

        while (m_socket.is_open()) {
            const std::byte* payload = nullptr;
            std::size_t payloadSize = 0;

            if (m_options.dont_read)
                co_await m_socket.async_wait(boost::asio::ip::tcp::socket::wait_read,
                                             boost::asio::use_awaitable);
            else {
                payloadSize = co_await m_socket.async_read_some(
                    boost::asio::buffer(m_read_buffer.get(), m_read_buffer.size()),
                    boost::asio::use_awaitable);
                payload = m_read_buffer.get();
            }

            /* close_impl() may have run while that read was in flight, and a read can genuinely
             * complete *with data* in that window: closing the socket cancels a pending operation,
             * not one the reactor has already satisfied. Since close_impl() runs on this strand
             * too, a closed socket here means it has already finished -- OnDisconnected has fired
             * and the gate has been released -- so reporting the read now would deliver data after
             * the disconnection, and parking on the gate below would park us for good, because
             * releasing a lease is a no-op once the session has stopped. Drop the read instead. */
            if (!m_socket.is_open())
                break;

            if (!m_callbacks.onDataAvailable)
                continue;

            /* Hand out a lease and park until it is dropped. This is what lets us pass the
             * reader's own buffer to a callback that may outlive its own return, and what stops
             * us reading ahead of a consumer that cannot keep up.
             *
             * The common case -- a callback that returns without moving the lease -- releases the
             * gate inline, so m_read_released is already true here and we never suspend. */
            m_read_released.store(false, std::memory_order::release);
            m_callbacks.onDataAvailable.invoke(*this, payload, payloadSize,
                                               read_lease(shared_from_this()));

            /* The socket is re-tested on every pass so that no lease can hold us past close_impl():
             * it runs on this strand, so if the socket is still open its release_read_gate() is
             * still to come and is guaranteed to wake us. */
            while (!m_read_released.load(std::memory_order::acquire) && m_socket.is_open())
                co_await m_read_gate.async_wait(
                    boost::asio::as_tuple(boost::asio::use_awaitable));
        }
    } catch (...) {
        /* if clean closing, do not throw error
         *
         * We have to do this because during graceful shutdown, close() will close the socket and so
         * cause the reader to throw.
         */
        if (m_state.load(std::memory_order::acquire) == state_t::running)
        {
            std::lock_guard lock(m_exception_mutex);
            if (!m_exception)
                m_exception = std::current_exception();
        }
    }

    close();
}

void tcp_session::release_read_gate() {
    m_read_released.store(true, std::memory_order::release);
    m_read_gate.cancel();
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
            }

            // make sure close() is called outside the m_write_mutex lock, as close() could call
            // the user-defined on-disconnection callback. If in the callback the user calls
            // write(), we'll have a deadlock as write() as locks that mutex
            if (buffers.empty()) {
                if (m_state.load(std::memory_order::acquire)!= state_t::running)
                    close();
                co_return;
            }

            std::vector<boost::asio::const_buffer> buffersAsio;
            buffersAsio.reserve(buffers.size());
            for (auto& buff : buffers)
                buffersAsio.emplace_back(buff.get(), buff.size());

            // SO_SNDTIMEO only applies for blocking calls, not async. Use cancel_after to enforce
            // the timeout.
            auto result = co_await boost::asio::async_write(
                m_socket, buffersAsio,
                boost::asio::cancel_after(std::chrono::milliseconds(timeoutMSec),
                                          boost::asio::as_tuple(boost::asio::use_awaitable)));

            // boost::asio::error::timed_out might be raised by async_write
            // boost::asio::error::operation_aborted will be raised by cancel_after
            if (auto ec = std::get<0>(result); ec) {
                if (ec == boost::asio::error::timed_out ||
                    ec == boost::asio::error::operation_aborted)
                    SG_THROW(exceptions::net::time_out);
                throw boost::system::system_error(ec);
            }
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
} // namespace sg::net
