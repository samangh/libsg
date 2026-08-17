#include "sg/net/tcp_server.h"
#include "sg/net/tcp_native.h"

#include "sg/debug.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/strand.hpp>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <iostream>

/* Notes:
 *
 *   - m_context->is_running() will return false as soon as a stop request is made, it'll
 *     return false event if there are still handlers running
 */

namespace {

/* Errors from accept() that describe the state of the process, or of the one connection being
 * accepted, and not that of the listening socket. The listener recovers from these by backing off
 * and accepting again; anything NOT named here is taken to be fatal to the acceptor and stops the
 * server.
 *
 * Linux passes network errors already pending on the *new* connection back out of accept(2), which
 * the man page is explicit should be retried like EAGAIN rather than treated as a fault of the
 * listening socket -- so the whole of that set is here too.
 */
bool is_transient_accept_error(const boost::system::error_code& ec) {
    return
        /* the process is out of some resource; retrying works once the pressure lifts */
        ec == boost::asio::error::no_descriptors ||                 // EMFILE
        ec == boost::system::errc::too_many_files_open_in_system || // ENFILE
        ec == boost::asio::error::no_buffer_space ||                // ENOBUFS
        ec == boost::asio::error::no_memory ||                      // ENOMEM
        ec == boost::system::errc::no_stream_resources ||           // ENOSR
        ec == boost::asio::error::interrupted ||                    // EINTR
        ec == boost::asio::error::try_again ||                      // EAGAIN
        ec == boost::asio::error::would_block ||                    // EWOULDBLOCK

        /* this one client went away between the SYN and the accept() */
        ec == boost::asio::error::connection_aborted || // ECONNABORTED
        ec == boost::asio::error::connection_reset ||   // ECONNRESET
        ec == boost::asio::error::timed_out ||          // ETIMEDOUT

        /* Network errors of the new connection, which Linux reports through accept(). Per
         * accept(2): "the application should detect the network errors defined for the protocol
         * after accept() and treat them like EAGAIN by retrying".
         *
         * EOPNOTSUPP is in that set, and also means "this socket is not SOCK_STREAM" -- which
         * would be fatal. It is treated as transient because bind_acceptors() only ever builds
         * tcp::acceptors, so the fatal reading cannot arise here. */
        ec == boost::asio::error::network_down ||           // ENETDOWN
        ec == boost::asio::error::network_unreachable ||    // ENETUNREACH
        ec == boost::asio::error::network_reset ||          // ENETRESET
        ec == boost::asio::error::host_unreachable ||       // EHOSTUNREACH
        ec == boost::asio::error::no_protocol_option ||     // ENOPROTOOPT
        ec == boost::asio::error::operation_not_supported || // EOPNOTSUPP
        ec == boost::system::errc::protocol_error ||        // EPROTO

        /* Windows reports a blocking call already in progress; harmless to retry, and it cannot
         * arise from a plain errno on POSIX */
        ec == boost::asio::error::in_progress || // WSAEINPROGRESS

        /* EHOSTDOWN and ENONET have no name in boost::system::errc, so they are matched on the
         * raw errno. POSIX only: on Windows an asio socket error is a WSA code in the system
         * category, and a bare errno value compared against it would be matching in the wrong
         * namespace -- a small errno constant can collide with an unrelated Win32 error. Neither
         * is a documented accept() error there in any case. */
#if !defined(_WIN32)
    #ifdef EHOSTDOWN
        ec == boost::system::error_code(EHOSTDOWN, boost::system::system_category()) ||
    #endif
    #ifdef ENONET
        ec == boost::system::error_code(ENONET, boost::system::system_category()) ||
    #endif
#endif
        false;
}

constexpr auto accept_retry_min = std::chrono::milliseconds(1);
constexpr auto accept_retry_max = std::chrono::milliseconds(500);

template<typename FuncT, typename... Args>
void run_callback(const std::string& name, FuncT func, Args&&... args) {
    try {
        func.invoke(std::forward<Args>(args)...);
    } catch (const std::exception& callbackEx) {
        std::cerr << "tcp_server: " << name << " callback exception: " << callbackEx.what()
                  << std::endl;
    } catch (...) {
        std::cerr << "tcp_server: " << name << " callback exception" << std::endl;
    }
}

} // namespace

namespace sg::net {

tcp_server::~tcp_server() noexcept(false) {
    if (running_in_callback_thread()) {
        std::cerr << "tcp_server: ~tcp_server() called from one of the server's own threads"
                  << std::endl;
        std::terminate();
    }

    stop_async();
    if (m_stopping_thread.joinable())
        m_stopping_thread.join();
}

void tcp_server::stop_async() {
    std::lock_guard lock(m_mutex_start_stop);

    /* if stop thread not started, start */
    if (m_stop_in_operation.exchange(true))
        return;

    m_stopping_thread = std::jthread([this]() {
        /* Socket operations should be run on the io_context, as they are not thread safe.
         *
         * If the context is not running we must close directly instead. That happens when we
         * are unwinding a start() that failed before the context came up. */
        const bool context_running = m_context && m_context->is_running();
        for (size_t i = 0; i < m_acceptors.size(); ++i) {
            /* bind_acceptors() publishes an acceptor and its retry timer together, so the two
             * vectors always have the same size and ordering */
            const auto& acceptor = m_acceptors[i];
            const auto& timer = m_accept_retry_timers[i];

            /* Cancelling the retry timer as well as closing the acceptor: closing an acceptor
             * does not disturb a timer, so a listener backing off after a transient accept()
             * failure would otherwise hold up the wait below for the length of its backoff. */
            auto close = [acceptor, timer]() {
                try {
                    acceptor->close();
                } catch (...) {}
                try {
                    timer->cancel();
                } catch (...) {}
            };

            if (context_running)
                boost::asio::post(acceptor->get_executor(), close);
            else
                close();
        }

        /* Wait until every listener coroutine has exited.
         *
         * Only wait while the context can still make progress. A context that
         * is gone, or that has been prematurely stopped, never resumes a
         * listener suspended in async_accept -- its queued handlers are simply
         * dropped -- so that listener never reaches the decrement at the end of
         * listener(). A start() that failed inside m_context->run() leaves
         * exactly that state. */
        if (m_context && m_context->is_running()) {
            for (size_t n; (n = m_acceptors_running_count.load(std::memory_order::acquire)) != 0; )
                m_acceptors_running_count.wait(n, std::memory_order::acquire);
        } else {
            // if we are skipping this, set to zero to tidy up
            m_acceptors_running_count.store(0, std::memory_order::release);
        }

        disconnect_all();

        /* wait until all clients disconnected and all callbacks called, etc */
        for (size_t n; (n = m_active_sessions.load(std::memory_order::acquire)) != 0; )
            m_active_sessions.wait(n, std::memory_order::acquire);

        if (m_context) {
            m_context->stop_async();
            m_context->wait_for_stop();
        }

        m_running.store(false);
        m_running.notify_all();
    });
}

void tcp_server::start(std::vector<end_point> endpoints, CallBacks callbacks, options_t options) {
    if (endpoints.empty())
        SG_THROW(std::invalid_argument, "tcp_server requires at least one endpoint to listen on");

    if (m_running.exchange(true))
        SG_THROW(std::runtime_error, "tcp_server is already running");
    m_running.notify_all();

    // if these are not zero, something has gone wrong in stopping our last run!
    assert(m_active_sessions == 0);
    assert(m_acceptors_running_count == 0);

    // Make sure start()/stop_async() can't be called simultaneously
    std::lock_guard lock(m_mutex_start_stop);

    try {
        m_options = options;
        m_callbacks = std::move(callbacks);
        m_stop_in_operation.store(false);
        m_last_id = 0;
        m_endpoints = endpoints;

        {
            std::lock_guard lock(m_error_mutex);
            m_last_error = nullptr;
        }

        /* clear acceptors before re-setting context, just in case their destructors need a valid
         * context */
        m_acceptors.clear();
        m_accept_retry_timers.clear();

        auto stoppedTask = std::bind(&tcp_server::on_io_pool_stopped, this, std::placeholders::_1);
        m_context = asio_io_pool::create(options.no_threads, false, stoppedTask);

        /* start listening sockets, this is the part likely yo throw */
        bind_acceptors();

        /* Each acceptor was bound to its own strand in bind_acceptors(); co_spawn the listener
         * onto that same strand so every operation on the acceptor stays serialised.
         *
         * None of these coroutines can execute yet -- they only start once the context is run,
         * at the end of this function. That ordering is what keeps start-up recoverable: an
         * exception anywhere above leaves bound sockets and queued coroutines, but nothing live. */
        for (size_t i = 0; i < m_acceptors.size(); ++i)
            boost::asio::co_spawn(m_acceptors[i]->get_executor(),
                                  listener(m_acceptors[i], m_accept_retry_timers[i]),
                                  boost::asio::detached);
        m_acceptors_running_count.store(m_acceptors.size(), std::memory_order::release);

        /* invoke this before the context starts processing accepts, so that OnStartedListening()
         * always precedes OnDataAvailable() */
        if (m_callbacks.OnStartedListening)
            m_callbacks.OnStartedListening.invoke(*this);

        m_context->run();
    } catch (...) {
        /* Nothing is left running once the pool is gone: either we never reached run(), so the
         * queued coroutines never executed, or run() itself failed -- and it stops the context
         * and joins its workers before rethrowing. Destroying the pool destroys the io_context
         * and with it those coroutine frames.
         *
         * clear acceptors before re-setting context, just in case their destructors need a valid
         * context */
        m_acceptors.clear();
        m_accept_retry_timers.clear();
        m_context.reset();

        /* needed in case m_context->run() throws */
        m_acceptors_running_count.store(0);

        m_running.store(false);
        m_running.notify_all();
        throw;
    }
}

void tcp_server::future_get_once() const noexcept(false) {
    /* Same deadlock as in ~tcp_server, but recoverable here: nothing is being destroyed, so
     * the caller can be told rather than hung. Mirrors tcp_session::wait_until_stopped(). */
    if (running_in_callback_thread())
        SG_THROW(std::logic_error,
                 "tcp_server::future_get_once() must not be called from a server callback: it "
                 "would wait for the thread it is running on. Use is_stopped() instead.");

    if (!m_context)
        return;

    m_context->wait_for_stop();
    m_running.wait(true);
}

bool tcp_server::is_stopped() const {
    return !m_running.load(std::memory_order::acquire);
}

bool tcp_server::running_in_callback_thread() const {
    /* Every server callback now runs on a thread m_context owns: the I/O workers run
     * OnSessionCreated / OnSessionDataAvailable / OnAcceptError and, through the session strand,
     * OnDisconnected; the stopped-callback thread runs OnStoppedListening. */
    return m_context && m_context->running_in_pool_thread();
}

size_t tcp_server::clients_count() const {
    std::shared_lock lock(m_mutex);
    return m_sessions.size();
}
std::map<tcp_server::session_id_t, tcp_server::ptr> tcp_server::sessions() const {
    std::shared_lock lock(m_mutex);
    return m_sessions;
}
void tcp_server::write(session_id_t id, std::string_view data) {
    std::shared_lock lock(m_mutex);
    m_sessions.at(id)->write(data);
}

void tcp_server::write(session_id_t id, const void* data, size_t size) {
    auto ptr = sg::make_shared_c_buffer<std::byte>(size);
    std::memcpy(ptr.get(), data, size);
    write(id, ptr);
}

void tcp_server::write(session_id_t id, sg::shared_c_buffer<std::byte> buffer) {
    std::shared_lock lock(m_mutex);
    m_sessions.at(id)->write(buffer);
}

void tcp_server::disconnect(session_id_t id) {
    ptr sess;
    {
        std::shared_lock lock(m_mutex);
        sess = m_sessions.at(id);
    }
    sess->stop_async();
}

void tcp_server::disconnect_all() {
    std::vector<ptr> sessions;
    {
        std::shared_lock lock(m_mutex);
        sessions.reserve(m_sessions.size());
        for (auto& sess : m_sessions | std::views::values) sessions.push_back(sess);
    }
    for (auto& sess : sessions) sess->stop_async();
}

tcp_server::ptr tcp_server::session(session_id_t id) {
    std::shared_lock lock(m_mutex);
    return m_sessions.at(id);
}

boost::asio::awaitable<void>
tcp_server::listener(std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor,
                     std::shared_ptr<boost::asio::steady_timer> retry_timer) {
    try {
        auto backoff = accept_retry_min;

        while (!m_stop_in_operation.load(std::memory_order::acquire)) {
            /* Accept onto the general io_context, not this acceptor's strand: the new
             * session must run across all workers. Passing the io_context explicitly
             * overrides the default of inheriting the acceptor's (strand) executor.
             *
             * The error is taken as an error_code rather than as an exception, so that a
             * per-connection failure can be told apart from one that is fatal to the acceptor. */
            boost::system::error_code ec;
            auto socket = co_await acceptor->async_accept(
                m_context->context(), boost::asio::redirect_error(boost::asio::use_awaitable, ec));

            if (ec) {
                if (!is_transient_accept_error(ec)) {
                    /* operation_aborted is how a listener is woken from async_accept by the
                     * close() that stop_async() posts; it is the normal way out, not a failure */
                    if (ec != boost::asio::error::operation_aborted)
                        record_error(std::make_exception_ptr(boost::system::system_error(ec)));

                    stop_async();
                    break;
                }

                /* A stop already under way is not an accept failure worth reporting */
                if (m_stop_in_operation.load(std::memory_order::acquire))
                    continue;

                if (backoff == accept_retry_min || backoff >= accept_retry_max) {
                    if (m_callbacks.OnAcceptError)
                        run_callback("OnAcceptError()", m_callbacks.OnAcceptError, *this,
                                     std::make_exception_ptr(boost::system::system_error(ec)));
                    else
                        std::cerr << "tcp_server: " << boost::system::system_error(ec).what()
                                  << std::endl;
                }

                retry_timer->expires_after(backoff);
                co_await retry_timer->async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));

                backoff = std::min(backoff * 2, accept_retry_max);
                continue;
            }

            backoff = accept_retry_min;

            /* Everything below is per-connection: a throw out of tcp_session::create(), or a
             * bad_alloc from m_sessions.emplace(), must cost us this one connection and not the
             * whole listener. The accepted socket is closed by its own destructor as the stack
             * unwinds. */
            try {
                auto id = m_last_id++;

                auto onSessionDisconnected = [this, id](tcp_session&, std::exception_ptr ex) {
                    on_session_stopped(id, ex);
                };

                auto onData = [this, id](tcp_session&, const std::byte* data, size_t size) {
                    inform_user_of_data(id, data, size);
                };

                tcp_session::Callbacks::OnConnected onConn = [this, id](tcp_session&) {
                    if (m_callbacks.OnSessionCreated)
                        m_callbacks.OnSessionCreated.invoke(*this, id);
                };

                auto sess = tcp_session::create(
                    m_context->context(),
                    std::move(socket),
                    tcp_session::Callbacks {
                        .onConnected = onConn,
                        .onDisconnected = onSessionDisconnected,
                        .onDataAvailable = onData
                    },
                    m_options.session_options);

                /* check that the accept did not return because stop_async was called */
                if (!m_stop_in_operation.load(std::memory_order::acquire)) {
                    {
                        std::unique_lock lock(m_mutex);
                        m_sessions.emplace(id, sess);
                        m_active_sessions.fetch_add(1, std::memory_order::release);
                    }

                    /* start() may throw if session setup fails (e.g. the peer reset
                     * the connection).
                     *
                     * No manual cleanup is needed: start()'s own failure path has
                     * already fired the session's on_disconnected callback, which
                     * cleans things up. */
                    sess->start();
                }
            } catch (...) {
            }
        }
    } catch (...) {
        /* A backstop only: the loop above turns accept and timer failures into error codes, so
         * nothing is expected to reach here. It stays because a detached coroutine that lets an
         * exception escape calls std::terminate(). */
        record_error(std::current_exception());
        stop_async();
    }

    m_acceptors_running_count.fetch_sub(1, std::memory_order::acq_rel);
    m_acceptors_running_count.notify_all();
}

void tcp_server::bind_acceptors() {
    for (const auto& e : m_endpoints) {
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address(e.ip), e.port);

        // We can't do this, as SO_REUSEADDR and SO_EXCLUSIVEADDRUSE  have to be set before bind()
        // auto a = std::make_shared<boost::asio::ip::tcp::acceptor>(m_context->context(), ep);

        // note: keep-alive and timeout will be inherited on some platforms (Linux) but not others
        // (Windows), so we set them on a per-session basis instead.

        /* Bind the acceptor (and its listener) to a dedicated strand. An acceptor is a
         * Boost.Asio I/O object and so con-current use in multiple threads is not thread-safe.;
         * with more than one io worker thread, an in-flight async_accept and the close() posted by
         * stop_async() could run on different workers at the same time. Routing every operation on
         * this acceptor through one strand serialises them without a mutex. */
        auto strand = boost::asio::make_strand(m_context->context());

        auto a = std::make_shared<boost::asio::ip::tcp::acceptor>(strand);
        a->open(ep.protocol());
        native::set_exclusive_addr_use(a->native_handle(), m_options.exclusive_address_use);
        native::set_reuse_address(a->native_handle(), m_options.reuse_address);
        a->bind(ep);
        a->listen();

        /* The listener's backoff timer goes on the same strand as its acceptor, so that the
         * timer and the acceptor are serialised against each other without a mutex.
         *
         * Published before its acceptor, and only once nothing above can throw, so that every
         * acceptor is guaranteed a timer at its own index even if a push_back throws. */
        auto t = std::make_shared<boost::asio::steady_timer>(strand);

        m_accept_retry_timers.push_back(t);
        m_acceptors.push_back(a);
    }
}

void tcp_server::on_io_pool_stopped(asio_io_pool&) {
    if (m_callbacks.OnStoppedListening)
        m_callbacks.OnStoppedListening.invoke(*this, last_error());
}

void tcp_server::record_error(std::exception_ptr ex) {
    if (!ex)
        return;

    std::lock_guard lock(m_error_mutex);
    if (!m_last_error)
        m_last_error = ex;
}

std::exception_ptr tcp_server::last_error() const {
    std::lock_guard lock(m_error_mutex);
    return m_last_error;
}


void tcp_server::inform_user_of_data(session_id_t id, const std::byte* data, size_t size) {
    if (m_callbacks.OnSessionDataAvailable)
        m_callbacks.OnSessionDataAvailable.invoke(*this, id, data, size);
}

void tcp_server::on_session_stopped(session_id_t id, std::exception_ptr ex) {
    // decrement, whatever happens
    struct release_session {
        tcp_server* server;
        ~release_session() {
            if (server->m_active_sessions.fetch_sub(1, std::memory_order::acq_rel) == 1)
                server->m_active_sessions.notify_all();
        }
    } release{this};

    if (m_callbacks.OnDisconnected)
        run_callback("OnDisconnected", m_callbacks.OnDisconnected, *this, id, ex);

    std::unique_lock lock(m_mutex);
    m_sessions.erase(id);
}
}
