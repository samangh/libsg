#include "sg/net/tcp_server.h"
#include "sg/net/tcp_native.h"

#include "sg/debug.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>

#include <algorithm>
#include <limits>

namespace sg::net {

namespace {
/* Lower bound on the pause threshold, whatever the socket reports. Large enough that any ordinary
 * message is delivered well inside it, small enough to be irrelevant per session. */
constexpr size_t min_pause_threshold = 64 * 1024;
}

tcp_server::tcp_server(private_tag, CallBacks callbacks, options_t options)
    : m_callbacks(std::move(callbacks)), m_options(options) {

    /* Up before anything can post to it. Guarded (the `true`) so it survives idle stretches between
     * sessions -- see the member's declaration. No stopped-callback: its stop is not a server event
     * and must not be confused with OnStoppedListening. */
    m_cb_pool = asio_io_pool::create(m_options.no_callback_threads, true, nullptr);
    m_cb_pool->run();

    /* Parked now, so stop_async() never has to create a thread. */
    m_teardown_thread = std::jthread([this]() {
        /* A stop requested from a callback during construction must not race bind_and_run()'s use
         * of m_context. */
        m_launch_done.wait(false, std::memory_order::acquire);
        m_stop_requested.wait(false, std::memory_order::acquire);

        /* An escaping exception would terminate the process, and leaving m_stopped unset would hang
         * every wait_until_stopped() -- including our own destructor's. */
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

    /* No try/catch: if this throws, ~tcp_server() cleans up. */
    server->bind_and_run(endpoints);

    return server;
}

tcp_server::~tcp_server() {
    stop_async();
    wait_until_stopped();

    /* teardown() already did this; repeated (a no-op once stopped) so that even a teardown() that
     * threw part-way cannot leave callback threads running into member destruction -- they touch
     * m_sessions, m_mutex and m_callbacks, all of which die below. */
    m_cb_pool->stop_async();
    m_cb_pool->wait_for_stop();
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
    /* Releases the teardown thread however we leave. Until then a stop requested by a user callback
     * below is recorded but not acted on, so it can neither be lost nor race our use of
     * m_context. */
    struct gate_release {
        std::atomic<bool>& flag;
        ~gate_release() {
            flag.store(true, std::memory_order::release);
            flag.notify_all();
        }
    } release{m_launch_done};

    auto stoppedTask = std::bind(&tcp_server::on_io_pool_stopped, this, std::placeholders::_1);
    m_context = asio_io_pool::create(m_options.no_io_threads, false, stoppedTask);

    /* Bind first. This is the part that throws (e.g. address in use), and it runs while the context
     * is idle, so a failure cannot leave a listener executing. */
    for (const auto& e : endpoints) {
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address(e.ip), e.port);

        /* An acceptor is not thread-safe, so with >1 io worker an in-flight async_accept and the
         * close() posted by teardown() could run concurrently; one strand serialises them without a
         * mutex.
         *
         * Keep-alive and timeout are deliberately not set here -- they are inherited on Linux but
         * not Windows, so they are set per-session instead. */
        auto strand = boost::asio::make_strand(m_context->context());

        auto a = std::make_shared<boost::asio::ip::tcp::acceptor>(strand);
        a->open(ep.protocol());
        native::set_exclusive_addr_use(a->native_handle(), m_options.exclusive_address_use);
        native::set_reuse_address(a->native_handle(), m_options.reuse_address);
        a->bind(ep);
        a->listen();

        m_acceptors.push_back(std::move(a));
    }

    /* Before anything is queued, so a throwing callback cannot orphan coroutines, and before the
     * context can process an accept, so OnStartedListening always precedes OnDataAvailable. The
     * callback receives *this, so from here the object is reachable by user code and may have a stop
     * requested on it. */
    if (m_callbacks.OnStartedListening)
        m_callbacks.OnStartedListening.invoke(*this);

    /* None can execute until run() below, so up to that point an exception leaves bound sockets and
     * queued coroutines but nothing live. use_future gives teardown() a join point for free. */
    for (const auto& a : m_acceptors)
        m_listeners_done.push_back(
            boost::asio::co_spawn(a->get_executor(), listener(a), boost::asio::use_future));

    /* Nothing below here may throw, or fail to record that the context is up -- teardown() decides
     * whether the listener futures are joinable on exactly this fact. */
    m_context->run();
    m_context_ran.store(true, std::memory_order::release);
}

void tcp_server::close_acceptors() {
    /* Socket operations are not thread-safe, so while the context is live the close goes through the
     * acceptor's own strand. When it is not live we must close directly, since a post() would never
     * execute.
     *
     * That is safe, though not for the reason "not running" suggests -- is_running() also reads
     * false in the pool's `stopping` state, where workers are not yet joined. What actually rules
     * out a concurrent touch: either the context never ran, so no worker ever existed, or it drained
     * naturally, meaning asio's work count reached zero and no listener is still parked in
     * async_accept. A drain is the only way to get here with a stopped context -- the one explicit
     * stop is in teardown(), after this call. */
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

    /* Only joinable if the context was brought up. Otherwise (launch() threw before run(), or run()
     * itself failed) the coroutines sit queued on a context that will never execute them, so their
     * promises can never be satisfied and get() would block forever. Dropping the futures is safe:
     * destroying a co_spawn future waits on nothing, and the frames die with the io_context.
     *
     * m_context_ran rather than is_running(), which cannot tell "never came up" from "already
     * drained". */
    if (m_context_ran.load(std::memory_order::acquire))
        for (auto& f : m_listeners_done)
            try {
                f.get();
            } catch (...) {}
    m_listeners_done.clear();

    disconnect_all();
    for (size_t n; (n = m_active_sessions.load(std::memory_order::acquire)) != 0;)
        m_active_sessions.wait(n, std::memory_order::acquire);

    /* Deliberately NOT guarded on is_running(). The pool has no work guard, so once the acceptors
     * are closed and the last session is gone it drains by itself and is_running() goes false while
     * its monitor thread is still about to invoke OnStoppedListening. Guarding would let teardown
     * finish before that callback ran. */
    if (m_context) {
        m_context->stop_async();
        m_context->wait_for_stop();
    }

    /* The close() posted by close_acceptors() is not guaranteed to have run: stopping the context
     * discards whatever is still queued, which is what happens when a listener finished without
     * ever reaching async_accept (a stop requested before the listeners first ran). On the ordinary
     * path the close is implicit -- a listener suspended in async_accept can only complete once it
     * has executed -- but here nothing forces it, and the socket would stay bound for the lifetime
     * of the object. */
    for (const auto& acceptor : m_acceptors)
        try {
            acceptor->close();
        } catch (...) {}

    /* Last, because the drain above is what makes it safe: stop_async() discards queued handlers
     * rather than running them (the work guard means the pool would never stop on its own, but also
     * that its stop is abrupt), and m_active_sessions reaching zero means every session strand has
     * already run its final handler.
     *
     * So wait_until_stopped() returning implies every session callback has completed. */
    m_cb_pool->stop_async();
    m_cb_pool->wait_for_stop();
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
    /* m_stop_requested is set once and never cleared, so no restart can reset it underneath a
     * running listener. */
    try {
        while (!m_stop_requested.load(std::memory_order::acquire)) {
            auto id = m_last_id++;

            /* Sequences a session's callbacks without serialising the whole server. Copied into
             * each of that session's callback lambdas, so it lives exactly as long as the session
             * and needs no server-side map. */
            const callback_strand_t cb_strand = boost::asio::make_strand(m_cb_pool->context());

            /* In its own variable rather than passed inline: argument evaluation order is
             * unspecified, and mixing that with a co_await in the same argument list is not worth
             * reasoning about. */
            auto callbacks = make_session_callbacks(id, cb_strand);

            /* Onto the general io_context rather than this acceptor's strand, so the new session can
             * run across all workers. Passing it explicitly overrides the default of inheriting the
             * acceptor's (strand) executor. */
            auto socket =
                co_await acceptor->async_accept(m_context->context(), boost::asio::use_awaitable);

            auto sess = tcp_session::create(std::move(socket), std::move(callbacks),
                                            m_options.session_options);

            /* the accept may have returned because we are shutting down */
            if (m_stop_requested.load(std::memory_order::acquire))
                break;

            {
                std::unique_lock lock(m_mutex);
                m_sessions.emplace(id, sess);
                m_active_sessions.fetch_add(1, std::memory_order::release);
            }

            /* start() throws if session setup fails (e.g. the peer reset the connection); contained
             * so one bad client cannot take the listener down. No manual cleanup needed --
             * start()'s failure path fires the session's on_disconnected callback, which does it. */
            try {
                sess->start();
            } catch (...) {}
        }
    } catch (...) {
        /* The accept failed, so unless we are already shutting down this endpoint is finished. With
         * a single endpoint the server would otherwise become a zombie: is_stopped() == false but
         * accepting nothing, and wait_until_stopped() blocking forever. Rethrow so the error also
         * reaches teardown() through our future.
         *
         * Note the normal shutdown path arrives here too, since closing the acceptor fails the
         * pending accept with operation_aborted -- stop_async() is already set and idempotent. */
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

tcp_session::Callbacks tcp_server::make_session_callbacks(session_id_t id,
                                                          const callback_strand_t& strand) {
    /* dont_read sessions read the socket inside OnSessionDataAvailable, and a native handle may only
     * be touched on the session's io strand, so that callback cannot move off the io threads.
     * OnSessionCreated must stay with it -- deferring one of the pair would let data be reported
     * before the session was. */
    const bool defer = !m_options.session_options.dont_read;

    tcp_session::Callbacks cb;

    if (defer) {
        /* Shared because its two ends run on different threads: bytes are added by the producer on
         * the session's io strand and released by the consumer on `strand`. Held by the lambdas, so
         * it lives exactly as long as the session.
         *
         * `limit` is the high-water mark at which the session is paused; 0 means "not resolved yet",
         * max() means "never pause". Written once, from the io strand. */
        struct flow_control {
            std::atomic<size_t> queued{0};
            std::atomic<size_t> limit{0};
        };
        auto flow = std::make_shared<flow_control>();

        cb.onConnected = [this, id, strand](tcp_session&) { on_session_created(id, strand); };

        cb.onDataAvailable = [this, id, strand,
                              flow](tcp_session& sess, const std::byte* data, size_t size) {
            /* async_read_some never reports a zero-length read (it surfaces as eof), but guard it
             * rather than call make_shared_c_buffer(0), which would turn a null malloc(0) into
             * bad_alloc. Nothing is lost by dropping an empty notification. */
            if (size == 0)
                return;

            /* Resolved from the socket rather than options_t, which is not the authority:
             * recv_buffer_size of 0 means "leave the OS default alone". Asking is legal in this one
             * place -- reader() calls us on the session's io strand, the only context where the
             * native handle may be touched -- and start() has applied the option by now, so the
             * answer reflects it (doubled on Linux, which is the kernel's business).
             *
             * Floored, because what the socket reports is not a useful mark on its own. Delivery is
             * asynchronous, so the queue is non-empty for as long as a callback is in flight; a mark
             * below one message therefore throttles a consumer that is keeping up perfectly, as soon
             * as a message spans more than one read. Platforms disagree about how small they will go
             * -- Linux clamps SO_RCVBUF to a couple of KiB, Windows honours a request of 1 byte
             * literally, which makes every read one byte and every mark one byte. */
            if (flow->limit.load(std::memory_order::acquire) == 0) {
                auto limit = std::numeric_limits<size_t>::max();
                try {
                    if (const int sz = native::get_recv_buffer_size(sess.native_handle()); sz > 0)
                        limit = std::max(static_cast<size_t>(sz), min_pause_threshold);
                } catch (...) {
                    /* a query we cannot trust must not throttle anyone */
                }
                flow->limit.store(limit, std::memory_order::release);
            }

            auto buffer = sg::make_shared_c_buffer<std::byte>(size);
            std::memcpy(buffer.get(), data, size);

            /* Must precede the post, or the handler could subtract before we have added. */
            const size_t queued = flow->queued.fetch_add(size, std::memory_order::acq_rel) + size;

            /* Back-pressure: once this much is queued undelivered, stop reading the socket. The
             * kernel buffer then fills, the window shuts and the peer is slowed rather than dropped.
             * The read that crossed the mark is still delivered, so the bound is the mark plus one
             * read.
             *
             * Ordered before the post for a second reason beyond the one above: pausing runs inline
             * (we are on the session's io strand, which is where pause_reading() dispatches to), so
             * it is in place before any consumer can subtract these bytes, and therefore before any
             * resume that subtraction triggers. A pause can never outlive the backlog that caused
             * it. */
            if (queued >= flow->limit.load(std::memory_order::acquire))
                sess.pause_reading();

            boost::asio::post(strand, [this, id, buffer, flow,
                                       weak_sess = sess.weak_from_this()]() {
                /* Releases however we leave, so a throwing user callback cannot strand the bytes and
                 * leave the peer throttled for someone else's bug. */
                struct release {
                    flow_control& flow;
                    const std::weak_ptr<tcp_session>& sess;
                    size_t bytes;
                    ~release() {
                        /* Resumed at half the pause mark: resuming at the mark itself would pause
                         * and resume once per message. Only the subtraction that crosses the mark
                         * sends it -- later ones would be no-ops, and with an unresolved limit
                         * (max(), so max()/2 is never reached from above) none is sent at all.
                         *
                         * Every pause is answered, because a paused session produces nothing: the
                         * backlog can only shrink, so some subtraction must cross the mark, and that
                         * subtraction is for bytes counted before the pause and so cannot run ahead
                         * of it. */
                        const size_t low = flow.limit.load(std::memory_order::acquire) / 2;
                        const size_t before = flow.queued.fetch_sub(bytes, std::memory_order::acq_rel);

                        if (before <= low || before - bytes > low)
                            return;

                        /* noexcept: we are a destructor, and dispatching allocates. */
                        try {
                            if (auto s = sess.lock())
                                s->resume_reading();
                        } catch (...) {}
                    }
                } releaser{*flow, weak_sess, buffer.size()};

                try {
                    inform_user_of_data(id, buffer.get(), buffer.size());
                } catch (...) {
                    fail_session(id);
                }
            });
        };
    } else {
        cb.onConnected = [this, id](tcp_session&) {
            if (m_callbacks.OnSessionCreated)
                m_callbacks.OnSessionCreated.invoke(*this, id);
        };

        cb.onDataAvailable = [this, id](tcp_session&, const std::byte* data, size_t size) {
            inform_user_of_data(id, data, size);
        };
    }

    /* Deferred for every session, dont_read or not: it runs the user's OnDisconnected and then
     * erases the session, and neither belongs on the session's io strand -- we are called from
     * inside that session's close handler. */
    cb.onDisconnected = [this, id, strand](tcp_session&, std::exception_ptr ex) {
        on_session_stopped(id, ex, strand);
    };

    return cb;
}

void tcp_server::fail_session(session_id_t id) noexcept {
    /* .at() throws if the session is already erased, and stop_async() is a no-op on one already
     * stopped, so both outcomes are fine. */
    try {
        disconnect(id);
    } catch (...) {}
}

void tcp_server::on_session_created(session_id_t id, const callback_strand_t& strand) {
    boost::asio::post(strand, [this, id]() {
        try {
            if (m_callbacks.OnSessionCreated)
                m_callbacks.OnSessionCreated.invoke(*this, id);
        } catch (...) {
            /* Keeps the contract that a throwing OnSessionCreated costs the session. This used to
             * hold for free, when the callback ran inside tcp_session::start() and the throw
             * unwound into the session's own failure path. */
            fail_session(id);
        }
    });
}

void tcp_server::on_session_stopped(session_id_t id, std::exception_ptr ex,
                                    const callback_strand_t& strand) {
    /* Called from tcp_session::close_impl(), i.e. on that session's io strand. Posting to the
     * callback strand gets OnDisconnected off the io threads and -- being the last thing posted for
     * this session -- puts the erase after every data callback, so session(id) is still valid
     * inside them.
     *
     * The decrement is last, so m_active_sessions reaching zero means every callback on every
     * session strand has finished. teardown() relies on exactly that.
     *
     * The erase cannot run the session's destructor: close_impl() is invoked through a lambda
     * holding a shared_from_this(), so ours is not the last reference. Whichever drops last
     * destroys the session, by which point it has stopped. */
    boost::asio::post(strand, [this, id, ex]() {
        try {
            if (m_callbacks.OnDisconnected)
                m_callbacks.OnDisconnected.invoke(*this, id, ex);
        } catch (...) {}

        {
            std::unique_lock lock(m_mutex);
            m_sessions.erase(id);
        }

        if (m_active_sessions.fetch_sub(1, std::memory_order::acq_rel) == 1)
            m_active_sessions.notify_all();
    });
}

} // namespace sg::net