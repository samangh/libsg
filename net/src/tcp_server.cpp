#include "sg/net/tcp_server.h"
#include "sg/net/tcp_native.h"

#include "sg/debug.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>

namespace sg::net {

tcp_server::tcp_server(private_tag, CallBacks callbacks, options_t options)
    : m_callbacks(std::move(callbacks)), m_options(options) {

    /* Park the teardown thread now, so that stop_async() never has to create one. */
    m_teardown_thread = std::jthread([this]() {
        /* Wait for launch() to finish before touching anything: a stop requested from a callback
         * during construction must not race bind_and_run()'s use of m_context. */
        m_launch_done.wait(false, std::memory_order::acquire);
        m_stop_requested.wait(false, std::memory_order::acquire);

        /* Contain everything. An exception escaping this lambda would terminate the process, and
         * leaving m_stopped unset would hang every wait_until_stopped() -- including the one in
         * our own destructor. */
        try {
            teardown();
        } catch (...) {
            //TODO: no way to report this yet; an error callback would be the place for it
        }

        m_stopped.store(true, std::memory_order::release);
        m_stopped.notify_all();
    });
}

std::unique_ptr<tcp_server> tcp_server::launch(std::vector<end_point> endpoints,
                                               CallBacks callbacks, options_t options) {
    if (endpoints.empty())
        SG_THROW(std::invalid_argument, "tcp_server requires at least one endpoint to listen on");

    auto server = std::make_unique<tcp_server>(private_tag{}, std::move(callbacks), options);

    /* Deliberately no try/catch: if this throws the destructor of server cleans up */
    server->bind_and_run(endpoints);

    return server;
}

tcp_server::~tcp_server() noexcept(false) {
    stop_async();
    wait_until_stopped();

    /* the session callbacks run here, so they must be drained after teardown() has finished */
    m_pool.wait_for_tasks();
}

void tcp_server::stop_async() {
    m_stop_requested.store(true, std::memory_order::release);
    m_stop_requested.notify_all();
}

void tcp_server::wait_until_stopped() const {
    m_stopped.wait(false, std::memory_order::acquire);
}

bool tcp_server::is_stopped() const { return m_stopped.load(std::memory_order::acquire); }

void tcp_server::bind_and_run(const std::vector<end_point>& endpoints) {
    /* Release the teardown thread when we leave, however we leave. Until then a stop requested by
     * a user callback below is recorded but not acted on, so it can neither be lost nor race our
     * own use of m_context. */
    struct gate_release {
        std::atomic<bool>& flag;
        ~gate_release() {
            flag.store(true, std::memory_order::release);
            flag.notify_all();
        }
    } release{m_launch_done};

    auto stoppedTask = std::bind(&tcp_server::on_io_pool_stopped, this, std::placeholders::_1);
    m_context = asio_io_pool::create(m_options.no_threads, false, stoppedTask);

    /* Bind first. This is the part that throws (e.g. the address is already in use), and it runs
     * while the context is idle, so a failure cannot leave a listener executing. */
    for (const auto& e : endpoints) {
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address(e.ip), e.port);

        /* One strand per acceptor: an acceptor is not thread-safe, so with more than one io worker
         * an in-flight async_accept and the close() posted by teardown() could otherwise run
         * concurrently. Routing both through one strand serialises them without a mutex.
         *
         * Note keep-alive and timeout are deliberately not set here: they are inherited on some
         * platforms (Linux) but not others (Windows), so they are set per-session instead. */
        auto strand = boost::asio::make_strand(m_context->context());

        auto a = std::make_shared<boost::asio::ip::tcp::acceptor>(strand);
        a->open(ep.protocol());
        native::set_exclusive_addr_use(a->native_handle(), m_options.exclusive_address_use);
        native::set_reuse_address(a->native_handle(), m_options.reuse_address);
        a->bind(ep);
        a->listen();

        m_acceptors.push_back(std::move(a));
    }

    /* Fire this before anything is queued, so that a throwing callback cannot leave orphaned
     * coroutines behind, and before the context can process an accept, so OnStartedListening
     * always precedes OnDataAvailable. Note the callback receives *this, so from here the object
     * is reachable by user code and may have a stop requested on it. */
    if (m_callbacks.OnStartedListening)
        m_callbacks.OnStartedListening.invoke(*this);

    /* Queue the listeners. None can execute until the context is run below, so up to that point an
     * exception leaves bound sockets and queued coroutines but nothing live. use_future gives us a
     * join point for teardown() at no extra cost. */
    for (const auto& a : m_acceptors)
        m_listeners_done.push_back(
            boost::asio::co_spawn(a->get_executor(), listener(a), boost::asio::use_future));

    /* Brings the listeners to life. Nothing below here may throw, or fail to record that the
     * context is up -- teardown() decides whether the listener futures are joinable or must be
     * abandoned on exactly this fact. */
    m_context->run();
    m_context_ran.store(true, std::memory_order::release);
}

void tcp_server::close_acceptors() {
    /* Socket operations belong on the io_context, as they are not thread-safe. If the context is
     * not running nothing can be executing on the acceptor's strand, so closing directly is both
     * safe and necessary -- a post() would never be executed. */
    const bool context_running = m_context && m_context->is_running();

    for (const auto& acceptor : m_acceptors) {
        if (context_running)
            boost::asio::post(acceptor->get_executor(), [acceptor]() {
                try {
                    acceptor->close();
                } catch (...) {}
            });
        else
            try {
                acceptor->close();
            } catch (...) {}
    }
}

void tcp_server::teardown() {
    close_acceptors();

    /* Join every listener -- but only if the context was actually brought up.
     *
     * Where it was not (a launch() that threw before run(), or run() itself failing) the listener
     * coroutines sit queued on a context that will never execute them, so their promises can never
     * be satisfied and get() would block forever. Dropping the futures instead is safe: destroying
     * a co_spawn future does not wait on anything, and the frames are destroyed along with the
     * io_context when the object is destroyed.
     *
     * m_context_ran is used rather than is_running() because is_running() cannot tell "never came
     * up" from "came up and has already drained": both report false. */
    if (m_context_ran.load(std::memory_order::acquire))
        for (auto& f : m_listeners_done)
            try {
                f.get();
            } catch (...) {}
    m_listeners_done.clear();

    disconnect_all();
    for (size_t n; (n = m_active_sessions.load(std::memory_order::acquire)) != 0;)
        m_active_sessions.wait(n, std::memory_order::acquire);

    /* Wait for the pool cycle to finish unconditionally -- deliberately NOT guarded on
     * is_running(). The pool has no work guard, so once the acceptors are closed and the last
     * session is gone it drains by itself and is_running() goes false while its monitor thread is
     * still about to invoke our stopped-callback (and hence OnStoppedListening). Guarding here
     * would let teardown finish before that callback ran.. */
    if (m_context) {
        m_context->stop_async();
        m_context->wait_for_stop();
    }

    /* The close() that close_acceptors() posted is not guaranteed to have run: stopping the
     * context discards whatever is still queued, and that is exactly what happens when a listener
     * finished without ever reaching async_accept -- a stop requested before the listeners first
     * ran. On the ordinary path the close is implicit, because a listener suspended in
     * async_accept can only complete once it has executed; on this path nothing forces it, and the
     * socket would stay bound for as long as the object lived. */
    for (const auto& acceptor : m_acceptors)
        try {
            acceptor->close();
        } catch (...) {}
}

size_t tcp_server::clients_count() const {
    std::shared_lock lock(m_mutex);
    return m_sessions.size();
}

std::map<tcp_server::session_id_t, tcp_server::ptr> tcp_server::sessions() const {
    std::shared_lock lock(m_mutex);
    return m_sessions;
}

tcp_server::ptr tcp_server::session(session_id_t id) {
    std::shared_lock lock(m_mutex);
    return m_sessions.at(id);
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
    std::shared_lock lock(m_mutex);
    m_sessions.at(id)->stop_async();
}

void tcp_server::disconnect_all() {
    std::shared_lock lock(m_mutex);
    for (auto& [_, sess] : m_sessions) sess->stop_async();
}

boost::asio::awaitable<void>
tcp_server::listener(std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor) {
    /* m_stop_requested is set once and never cleared, so unlike sg::net::tcp_server there is no
     * restart that could reset it underneath a running listener. */
    try {
        while (!m_stop_requested.load(std::memory_order::acquire)) {
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

            /* Accept onto the general io_context rather than this acceptor's strand: the new
             * session must be able to run across all workers. Passing the context explicitly
             * overrides the default of inheriting the acceptor's (strand) executor. */
            auto sess = tcp_session::create(
                co_await acceptor->async_accept(m_context->context(), boost::asio::use_awaitable),
                tcp_session::Callbacks{.onConnected = onConn,
                                       .onDisconnected = onSessionDisconnected,
                                       .onDataAvailable = onData},
                m_options.session_options);

            /* the accept may have returned because we are shutting down */
            if (m_stop_requested.load(std::memory_order::acquire))
                break;

            {
                std::unique_lock lock(m_mutex);
                m_sessions.emplace(id, sess);
                m_active_sessions.fetch_add(1, std::memory_order::release);
            }

            /* start() may throw if session setup fails (e.g. the peer reset the connection).
             * Contain it here so that one bad client cannot take the listener down with it. No
             * manual cleanup is needed: start()'s own failure path fires the session's
             * on_disconnected callback, which does it. */
            try {
                sess->start();
            } catch (...) {}
        }
    } catch (...) {
        /* The accept failed. Unless we are already shutting down this endpoint is finished, and
         * with a single endpoint the server would otherwise become a zombie: still reporting
         * is_stopped() == false but accepting nothing, with wait_until_stopped() blocking forever.
         * So treat it the way sg::net::tcp_server does and take the whole server down.
         *
         * Rethrow so that the error also reaches teardown() through our future.
         *
         * note: closing the acceptor makes the pending accept fail with operation_aborted, so the
         * normal shutdown path arrives here too -- stop_async() is already set and idempotent. */
        stop_async();
        throw;
    }
}

void tcp_server::on_io_pool_stopped(asio_io_pool&) {
    if (m_callbacks.OnStoppedListening)
        m_callbacks.OnStoppedListening.invoke(*this);
}

void tcp_server::inform_user_of_data(session_id_t id, const std::byte* data, size_t size) {
    if (m_callbacks.OnSessionDataAvailable)
        m_callbacks.OnSessionDataAvailable.invoke(*this, id, data, size);
}

void tcp_server::on_session_stopped(session_id_t id, std::exception_ptr ex) {
    /* This has to happen on another thread to escape the following deadlock:
     *
     *   1) the client disconnects, so this is called via the session's on_disconnection callback
     *   2) we erase the session, which runs the session's destructor
     *   3) that destructor waits for the session to end, which cannot happen while the
     *      on_disconnection callback is still running
     */
    m_pool.enqueue_detach([this, id, ex]() {
        if (m_callbacks.OnDisconnected)
            m_callbacks.OnDisconnected.invoke(*this, id, ex);

        {
            std::unique_lock lock(m_mutex);
            m_sessions.erase(id);
        }

        if (m_active_sessions.fetch_sub(1, std::memory_order::acq_rel) == 1)
            m_active_sessions.notify_all();
    });
}

} // namespace sg::net