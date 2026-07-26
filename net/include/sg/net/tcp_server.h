#pragma once
#include <sg/export/net.h>

#include "asio_io_pool.h"
#include "net.h"
#include "tcp_session.h"

#include "sg/buffer.h"
#include "sg/callback.h"
#include "sg/worker.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

#include <atomic>
#include <future>
#include <map>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace sg::net {

/** A callback-driven TCP server.
 *
 *  launch() returns a server that is already listening; destroying it stops it. There is no
 *  start()/stop()/start() lifecycle -- to restart, drop the object and launch a new one. Hence the
 *  stop flag is set once and never cleared, and a stop can never race a start.
 *
 *  Callback threads:
 *
 *      OnStartedListening       the caller's thread, before launch() returns
 *      OnStoppedListening       the io pool's monitor thread
 *      OnSessionCreated         \
 *      OnSessionDataAvailable    > the callback pool, on that session's strand
 *      OnDisconnected           /
 *
 *  Notes:
 *
 *   - sessions are handed out as shared_ptr so a caller holding one cannot have it destroyed
 *     underneath them.
 *
 *   - a session's three callbacks share one strand, so they are never concurrent and arrive in
 *     event order. Sessions run concurrently only if options_t::no_callback_threads > 1. Since no
 *     session callback occupies an io thread, all may block -- including on
 *     tcp_session::wait_until_stopped(), set_keepalive() and set_timeout(), which need io threads to
 *     progress.
 *
 *     Except under tcp_session::options_t::dont_read: the user reads the socket inside
 *     OnSessionDataAvailable, and a native handle may only be touched on the session's io strand, so
 *     it and OnSessionCreated are invoked synchronously on an io thread and must not block.
 *     OnDisconnected is posted as usual.
 *
 *   - a throwing session callback drops that session (the user still gets an OnDisconnected), but
 *     the exception is swallowed rather than forwarded: letting it escape a callback-pool worker
 *     would terminate the process. OnDisconnected's exception_ptr reports only session io errors.
 *
 *   - OnSessionDataAvailable applies back-pressure. Delivery is asynchronous -- the reader copies and
 *     posts -- so a peer can outrun its consumer; once the threshold's worth of bytes is queued
 *     undelivered the session stops reading its socket (tcp_session::pause_reading()), which fills the
 *     kernel receive buffer, shuts the advertised window and slows the peer. Reading resumes once the
 *     backlog is back under half the threshold. Raise recv_buffer_size to tolerate more bursting
 *     before the peer feels it.
 *
 *     The read that crossed the threshold is still delivered, so the queue is bounded by the
 *     threshold plus one read.
 *
 *     The threshold is what the socket reports as its receive buffer, subject to a floor of 64 KiB.
 *     The floor matters: the queue stays non-empty while a callback is in flight, so a threshold below
 *     one message would throttle a consumer that is keeping up as soon as a message spanned two
 *     reads. Platforms differ here -- Linux clamps SO_RCVBUF to a couple of KiB, Windows honours a
 *     request of 1 byte literally.
 *
 *     Note what this costs: a consumer that never returns stalls its peer indefinitely instead of
 *     losing its connection. Nothing times a paused session out.
 *
 *   - any callback may call stop_async(); none may call wait_until_stopped(), which would wait on
 *     the teardown it is part of (and for OnStartedListening, teardown cannot even begin until
 *     launch() returns).
 */
class SG_NET_EXPORT tcp_server {
    struct private_tag {
        explicit private_tag() = default;
    };

  public:
    typedef size_t session_id_t;
    typedef std::shared_ptr<tcp_session> ptr;

    CREATE_CALLBACK(started_listening_cb_t, void(tcp_server&))
    CREATE_CALLBACK(stopped_listening_cb_t, void(tcp_server&))
    CREATE_CALLBACK(session_created_cb_t, void(tcp_server&, session_id_t))
    CREATE_CALLBACK(session_data_available_cb_t, void(tcp_server&, session_id_t, const std::byte*, size_t))
    CREATE_CALLBACK(session_disconnected_cb_t, void(tcp_server&, session_id_t, std::exception_ptr))

    struct CallBacks {
        started_listening_cb_t OnStartedListening;
        stopped_listening_cb_t OnStoppedListening;
        session_created_cb_t OnSessionCreated;
        session_data_available_cb_t OnSessionDataAvailable;
        session_disconnected_cb_t OnDisconnected;
    };

    struct options_t {
        // work around bug https://github.com/llvm/llvm-project/issues/36032
        options_t() {};

        // server-specific options
        bool reuse_address{LIBSG_NET_REUSEADDR_DEFAULT};
        bool exclusive_address_use{LIBSG_NET_EXCLUSIVEADDRUSE_DEFAULT}; // Only used in Windows

        size_t no_io_threads{1};
        size_t no_callback_threads{1};

        //options that will apply to child sockets
        tcp_session::options_t session_options{};
    };

    /** Binds and starts listening on every endpoint.
     *
     *  Throws std::invalid_argument if @p endpoints is empty, or the underlying socket error if an
     *  endpoint cannot be bound; either way nothing is left running. On return OnStartedListening
     *  has fired and no connection can yet have been accepted. */
    [[nodiscard]] static std::unique_ptr<tcp_server> launch(std::vector<end_point> endpoints,
                                                            CallBacks callbacks,
                                                            options_t options = options_t());

    tcp_server(private_tag, CallBacks callbacks, options_t options);
    ~tcp_server();

    /* Listener coroutines and the teardown thread capture `this`. */
    tcp_server(const tcp_server&) = delete;
    tcp_server& operator=(const tcp_server&) = delete;
    tcp_server(tcp_server&&) = delete;
    tcp_server& operator=(tcp_server&&) = delete;

    /** Requests a stop. Never blocks, is idempotent, and may be called from any thread including
     *  from inside a callback. */
    void stop_async();

    /** Blocks until the server has fully stopped. Must not be called from a callback. */
    void wait_until_stopped() const;

    /** True once teardown is complete -- still false between stop_async() and the end of teardown. */
    [[nodiscard]] bool is_stopped() const;

    [[nodiscard]] size_t clients_count() const;
    [[nodiscard]] ptr session(session_id_t id);
    [[nodiscard]] std::map<session_id_t, ptr> sessions() const;

    void write(session_id_t id, std::string_view data);
    void write(session_id_t id, const void* data, size_t size);
    void write(session_id_t id, sg::shared_c_buffer<std::byte> buffer);

    void disconnect(session_id_t id);
    void disconnect_all();

  private:
    /* Lives on m_cb_pool, not m_context, so a callback never occupies an io thread. */
    using callback_strand_t = boost::asio::strand<boost::asio::io_context::executor_type>;

    /* Declaration order below is load-bearing, because members are destroyed in reverse: every asio
     * object must be destroyed before the context it belongs to, and both thread owners (m_cb_pool,
     * m_teardown_thread) must be joined before anything they touch. */
    CallBacks m_callbacks;
    options_t m_options;

    std::atomic<size_t> m_active_sessions{0};
    std::atomic<size_t> m_last_id{0};

    /* Serves the session callbacks. Run by the constructor, stopped by teardown() once the last
     * session is gone.
     *
     * MUST have a work guard: a guardless pool stops itself the moment it runs out of work, which
     * for a callback pool just means "no session is doing anything". Every later post would then be
     * dropped and teardown()'s wait for m_active_sessions never finish. The io pool gets away
     * without a guard only because a listener is always parked in async_accept.
     *
     * FIRST of the two pools, so it is destroyed LAST. A session's callbacks hold a
     * callback_strand_t, and destroying a strand touches its io_context, so this pool has to outlive
     * every session. That is not only the sessions in m_sessions: ~asio_io_pool() runs the
     * io_context's shutdown, which destroys the handlers still queued on it, and one of those is the
     * reader's coroutine frame holding the last shared_ptr to a session. So the session dies inside
     * ~m_context, and m_cb_pool must still be alive at that point.
     *
     * Its threads also touch m_sessions, m_mutex and m_callbacks, so it must additionally be
     * *stopped* before member destruction begins -- teardown() does that, and the destructor repeats
     * it unconditionally. */
    std::shared_ptr<sg::net::asio_io_pool> m_cb_pool;

    std::shared_ptr<sg::net::asio_io_pool> m_context;

    /* After m_context, so both are destroyed before it: an acceptor holds a strand referring to the
     * context, and a tcp_session holds a socket on it.
     *
     * Kept for the whole run because teardown() must reach the acceptors from outside the listener
     * coroutines -- closing one is the only way to break a listener parked in async_accept. Hence
     * shared_ptr: this vector, the listener's coroutine frame and the close() posted by
     * close_acceptors() each keep an acceptor alive, and their lifetimes do not nest. */
    std::vector<std::shared_ptr<boost::asio::ip::tcp::acceptor>> m_acceptors;
    mutable std::shared_mutex m_mutex;
    std::map<session_id_t, ptr> m_sessions;

    /* One future per listener coroutine -- the join point for teardown(). */
    std::vector<std::future<void>> m_listeners_done;

    /* Set once, never cleared: listeners poll it, the teardown thread waits on it. */
    std::atomic<bool> m_stop_requested{false};
    std::atomic<bool> m_stopped{false};

    /* Gates the teardown thread until the object is fully built: OnStartedListening can request a
     * stop while launch() is still running, and that must be honoured but not acted on until
     * launch() has stopped touching m_context and m_acceptors. Released by bind_and_run() on every
     * exit path. */
    std::atomic<bool> m_launch_done{false};

    /* Set once bind_and_run() has run the io context. teardown() must distinguish "never came up"
     * from "came up and already drained" -- is_running() reports false for both -- because only in
     * the former are the listener futures unsatisfiable. */
    std::atomic<bool> m_context_ran{false};

    /* Parked from construction until a stop is requested, so stop_async() only has to set a flag:
     * no thread to create, hence no race between two callers and nothing for the destructor to
     * assign. */
    std::jthread m_teardown_thread;

    void bind_and_run(const std::vector<end_point>& endpoints);
    void teardown();
    void close_acceptors();

    /* Co-owns its acceptor, so it cannot be destroyed while an accept is in flight. */
    boost::asio::awaitable<void>
    listener(std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor);

    void on_io_pool_stopped(asio_io_pool&);
    void inform_user_of_data(session_id_t id, const std::byte* data, size_t size);

    /* Drops a session whose callback threw. No-op if the session has already gone. */
    void fail_session(session_id_t id) noexcept;

    /* Both take the strand rather than looking it up: it is captured by the session's own callback
     * lambdas, so there is no server-side map to consult. */
    void on_session_created(session_id_t id, const callback_strand_t& strand);
    void on_session_stopped(session_id_t id, std::exception_ptr ex,
                            const callback_strand_t& strand);

    /* Callbacks that copy incoming data and post the user's callbacks onto @p strand. */
    tcp_session::Callbacks make_session_callbacks(session_id_t id,
                                                 const callback_strand_t& strand);
};

}
