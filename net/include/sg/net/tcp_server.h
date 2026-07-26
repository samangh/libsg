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

#include <atomic>
#include <future>
#include <map>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <vector>

#include <thread_pool/thread_pool.h>

namespace sg::net {

/** A callback-driven TCP server.
 *
 *  launch() returns a server that is already listening, and destroying it stops it. There is
 *  deliberately no start()/stop()/start() lifecycle: a server object is either running or being
 *  torn down. To restart, drop the object and launch a new one.
 *
 *  That shape is what keeps the implementation tractable. While a server is being built nothing
 *  else can reach it -- the caller does not have the pointer yet -- so a stop can never race a
 *  start, and no member can carry stale state over from a previous run because there is no previous
 *  run. In particular the stop flag is set once and never cleared.
 *
 *  notes:
 *
 *   - we store sessions as shared_ptr, so that when they user acquires them through the
 *     `sessions()` or `session(..)` function, they can be sure about the lifetime of the object
 *     (i.e. the returned session will exist and not get destructed whilst they have the
 *     shared_ptr)
 *
 *   - callbacks are invoked from an io thread or from the internal callback pool, never from the
 *     thread that called launch(). A callback may call stop_async(), but must not call
 *     wait_until_stopped(): that would wait for the teardown it is itself part of.
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
        size_t no_threads{1};

        //options that will apply to child sockets
        tcp_session::options_t session_options{};
    };

    /** Binds and starts listening on every endpoint.
     *
     *  Throws std::invalid_argument if @p endpoints is empty, or the underlying socket error if an
     *  endpoint cannot be bound; either way nothing is left running. By the time this returns
     *  OnStartedListening has fired and no connection can yet have been accepted. */
    [[nodiscard]] static std::unique_ptr<tcp_server> launch(std::vector<end_point> endpoints,
                                                            CallBacks callbacks,
                                                            options_t options = options_t());

    tcp_server(private_tag, CallBacks callbacks, options_t options);

    /** Stops the server and blocks until every listener, session and callback has finished.
     *
     *  noexcept(false) because the final drain of the callback pool can throw, and swallowing that
     *  silently would hide a real failure. */
    ~tcp_server() noexcept(false);

    /* Listener coroutines and the teardown thread capture `this`, so the object cannot move. */
    tcp_server(const tcp_server&) = delete;
    tcp_server& operator=(const tcp_server&) = delete;
    tcp_server(tcp_server&&) = delete;
    tcp_server& operator=(tcp_server&&) = delete;

    /** Requests a stop. Never blocks, is idempotent, and may be called from any thread including
     *  from inside a callback. */
    void stop_async();

    /** Blocks until the server has fully stopped. Must not be called from a callback. */
    void wait_until_stopped() const;

    /** True once teardown is complete. Note this is still false between a stop_async() and the end
     *  of that teardown. */
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
    /* Declaration order below is load-bearing, because members are destroyed in reverse: every
     * asio object must be destroyed before the context it belongs to, and both thread owners
     * (m_pool, m_teardown_thread) must be joined before anything they touch. */
    CallBacks m_callbacks;
    options_t m_options;

    std::atomic<size_t> m_active_sessions{0};
    std::atomic<size_t> m_last_id{0};

    std::shared_ptr<sg::net::asio_io_pool> m_context;

    /* After m_context, so that both are destroyed before it: an acceptor holds a strand referring
     * to the context, and a tcp_session holds a socket on it.
     *
     * m_acceptors is kept for the whole run because teardown() has to reach the acceptors from
     * outside the listener coroutines: closing one is the only way to break a listener parked in
     * async_accept, and once the pool has stopped teardown closes them directly through this
     * vector. Hence shared_ptr rather than sole ownership -- the vector, the listener's coroutine
     * frame and the close() posted by close_acceptors() each have to keep an acceptor alive, and
     * their lifetimes do not nest. */
    std::vector<std::shared_ptr<boost::asio::ip::tcp::acceptor>> m_acceptors;
    mutable std::shared_mutex m_mutex;
    std::map<session_id_t, ptr> m_sessions;

    /* One future per listener coroutine -- the join point for teardown(). */
    std::vector<std::future<void>> m_listeners_done;

    /* The single stop flag. Set once, never cleared: listeners poll it to decide whether to keep
     * accepting, and the teardown thread waits on it. */
    std::atomic<bool> m_stop_requested{false};
    std::atomic<bool> m_stopped{false};

    /* Gates the teardown thread until the object is fully built. A user callback fired from
     * bind_and_run() (OnStartedListening) can request a stop while launch() is still running; the
     * request must be honoured, but must not be acted on until launch() has stopped touching
     * m_context and m_acceptors. Released by bind_and_run() on every exit path. */
    std::atomic<bool> m_launch_done{false};

    /* Set once bind_and_run() has successfully run the io context, and never cleared. teardown()
     * needs to distinguish "the context never came up" from "the context came up and has already
     * drained" -- asio_io_pool::is_running() reports false for both -- because only in the former
     * case are the listener futures unsatisfiable. */
    std::atomic<bool> m_context_ran{false};

    dp::thread_pool<> m_pool{1};

    /* Created by the constructor and parked until a stop is requested. Existing for the whole
     * lifetime means stop_async() only has to set a flag: there is no thread to create, hence no
     * race between two callers of stop_async() and nothing for the destructor to assign. */
    std::jthread m_teardown_thread;

    void bind_and_run(const std::vector<end_point>& endpoints);
    void teardown();
    void close_acceptors();

    /* Co-owns its acceptor, so it cannot be destroyed while an accept is in flight. */
    boost::asio::awaitable<void>
    listener(std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor);

    void on_io_pool_stopped(asio_io_pool&);
    void inform_user_of_data(session_id_t id, const std::byte* data, size_t size);
    void on_session_stopped(session_id_t id, std::exception_ptr ex);
};

}
