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
#include <boost/asio/steady_timer.hpp>

#include <exception>
#include <map>
#include <mutex>

namespace sg::net {


class SG_NET_EXPORT tcp_server {
    /* notes:
     *
     *   - we store sessions as shared_ptr, so that when they user acquires them through the
     *     `sessions()` or `session(..)` function, they can be sure about the lifetime of the object
     *     (i.e. the returned session will exist and not get destructed whilst they have the
     *     shared_ptr)
     *
     *   - the start() has two overloads rather than taking `options_t options = options_t()`. This
     *     is needed because options_t is defined in tcp_server.
     */

  public:
    typedef size_t session_id_t;
    typedef std::shared_ptr<tcp_session> ptr;

    CREATE_CALLBACK(started_listening_cb_t, void(tcp_server&))
    /** Fired once the server has stopped listening. The exception_ptr holds the error that
     *  stopped it, or is null if it stopped cleanly (i.e. through stop_async()). */
    CREATE_CALLBACK(stopped_listening_cb_t, void(tcp_server&, std::exception_ptr))
    /** Fired when a socket accept() fails in a way that is recoverable, and the listener will
     *  retry. For example, if the process is out of file descriptors.
     *
     *  Fires on the first such failure, and then keeps firing for as long as the condition
     *  lasts -- but at most a couple of times a second, however fast the accepts fail.
     *
     *  Only a known set of conditions counts as recoverable. Everything else -- including an
     *  error the listener does not recognise -- is treated as fatal to the acceptor: it does not
     *  come through here, it stops the server, and it is reported by OnStoppedListening() /
     *  last_error() instead. */
    CREATE_CALLBACK(accept_error_cb_t, void(tcp_server&, std::exception_ptr))
    /** Consulted for each connection the OS has accepted, before any session exists. Return
     *  false to drop it: no id is taken, no other callback follows, and the peer sees the
     *  connection close at once.
     *
     *  Runs on the acceptor's I/O thread and holds up the next accept, so keep it quick. A
     *  filter that throws rejects the connection. */
    CREATE_CALLBACK(should_accept_cb_t, bool(tcp_server&, end_point))
    CREATE_CALLBACK(session_created_cb_t, void(tcp_server&, session_id_t))
    CREATE_CALLBACK(session_data_available_cb_t, void(tcp_server&, session_id_t, const std::byte*, size_t))
    CREATE_CALLBACK(session_disconnected_cb_t, void(tcp_server&, session_id_t, std::exception_ptr))

    struct CallBacks {
        started_listening_cb_t OnStartedListening;
        stopped_listening_cb_t OnStoppedListening;
        accept_error_cb_t OnAcceptError;
        should_accept_cb_t ShouldAccept;
        session_created_cb_t OnSessionCreated;
        session_data_available_cb_t OnSessionDataAvailable;
        session_disconnected_cb_t OnDisconnected;
    };

    struct options_t {
        bool reuse_address{LIBSG_NET_REUSEADDR_DEFAULT};
        bool exclusive_address_use{LIBSG_NET_EXCLUSIVEADDRUSE_DEFAULT}; // Only used in Windows
        size_t no_threads{1};

        //options that will apply to child sockets
        tcp_session::options_t session_options{};
    };

    ~tcp_server();

    void start(std::vector<end_point> endpoints, CallBacks callbacks, options_t options);
    void start(std::vector<end_point> endpoints, CallBacks callbacks);

    void stop_async();
    void future_get_once() const;
    bool is_stopped() const;

    /** returns being listened on, in the order they were given to @c start().
     *
     * Valid from inside @c OnStartedListening() onwards. If an endport with port 0 was assigned,
     * the actual port number chosen by the OS will be reported. */
    [[nodiscard]] std::vector<end_point> local_endpoints() const;

    /** True if the calling thread is one this server runs its callbacks on.  */
    [[nodiscard]] bool running_in_callback_thread() const;

    /** @brief The error that stopped the listener, or @c nullptr if it stopped cleanly.
     *
     * Set before OnStoppedListening() fires and cleared by the next start(), so it is valid to
     * query from inside that callback and at any point afterwards until the server is restarted.
     */
    [[nodiscard]] std::exception_ptr last_error() const;

    size_t clients_count() const;

    /** @brief Looks a session up by id.
     *
     * @throws sg::exceptions::net::session_not_found if no session has that id.
     *
     * A session is erased once it has stopped, so an id kept from an earlier call can name a
     * session that has since gone -- a peer can disconnect at any time. Inside that session's own
     * OnSessionDataAvailable the id is stable, because the data callback and the session's
     * teardown share its strand; anywhere else, be ready for this to throw. To act on a session
     * over several steps, hold the returned @c shared_ptr rather than re-looking up the id. */
    ptr session(session_id_t id);
    std::map<session_id_t, ptr> sessions() const;

    /** @throws sg::exceptions::net::session_not_found if no session has that id; see session(). */
    void write(session_id_t id, std::string_view data);
    /** @throws sg::exceptions::net::session_not_found if no session has that id; see session(). */
    void write(session_id_t id, const void* data, size_t size);
    /** @throws sg::exceptions::net::session_not_found if no session has that id; see session(). */
    void write(session_id_t id, sg::shared_c_buffer<std::byte> buffer);

    /** @throws sg::exceptions::net::session_not_found if no session has that id; see session(). */
    void disconnect(session_id_t id);
    void disconnect_all();

  private:
    std::atomic<bool> m_running {false};

    mutable std::shared_mutex m_mutex;
    std::map<session_id_t, ptr> m_sessions;
    std::atomic<size_t> m_active_sessions{0};
    std::atomic<size_t> m_last_id{0};

    std::vector<end_point> m_endpoints;

    /* What the acceptors actually bound to. This is not m_endpoints if a port of 0 was asked for */
    std::vector<end_point> m_local_endpoints;

    std::shared_ptr<sg::net::asio_io_pool> m_context;

    //m_acceptors are kept for use by set_keepalive/set_timeout
    std::vector<std::shared_ptr<boost::asio::ip::tcp::acceptor>> m_acceptors;
    /* One retry timer per acceptor, in the same order as m_acceptors and sharing that acceptor's
     * strand. Used by listener() to back off after a transient accept() failure. */
    std::vector<std::shared_ptr<boost::asio::steady_timer>> m_accept_retry_timers;
    std::atomic<size_t> m_acceptors_running_count{0};

    CallBacks m_callbacks;
    options_t m_options;

    mutable std::mutex m_error_mutex;
    std::exception_ptr m_last_error;

    /* Serialises start()/stop_async() */
    std::mutex m_mutex_start_stop;
    std::atomic<bool> m_stop_in_operation = true;
    std::jthread m_stopping_thread; // declared last so it's joined first

    boost::asio::awaitable<void> listener(std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor,
                                          std::shared_ptr<boost::asio::steady_timer> retry_timer);

    /* Shared lookup for the id-taking public functions; throws session_not_found. */
    [[nodiscard]] ptr find_session(session_id_t id) const;

    void bind_acceptors();
    void on_io_pool_stopped(asio_io_pool&);

    /* Records the reason the listener is stopping. First writer wins: a fatal error on one
     * acceptor stops the others, and their resulting errors must not mask the original cause. */
    void record_error(std::exception_ptr ex);

    void inform_user_of_data(session_id_t id, const std::byte* data, size_t size);
    void on_session_stopped(session_id_t id,  std::exception_ptr ex);

};

}

