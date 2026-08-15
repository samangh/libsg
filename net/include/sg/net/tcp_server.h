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

#include <thread_pool/thread_pool.h>

namespace sg::net {


class SG_NET_EXPORT tcp_server {
    /* notes:
     *
     *   - we store sessions as shared_ptr, so that when they user acquires them through the
     *     `sessions()` or `session(..)` function, they can be sure about the lifetime of the object
     *     (i.e. the returned session will exist and not get destructed whilst they have the
     *     shared_ptr)
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
    CREATE_CALLBACK(session_created_cb_t, void(tcp_server&, session_id_t))
    CREATE_CALLBACK(session_data_available_cb_t, void(tcp_server&, session_id_t, const std::byte*, size_t))
    CREATE_CALLBACK(session_disconnected_cb_t, void(tcp_server&, session_id_t, std::exception_ptr))

    struct CallBacks {
        started_listening_cb_t OnStartedListening;
        stopped_listening_cb_t OnStoppedListening;
        accept_error_cb_t OnAcceptError;
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

    ~tcp_server() noexcept(false);

    void start(std::vector<end_point> endpoints, CallBacks callbacks,
               options_t options = options_t());

    void stop_async();
    void future_get_once() const;
    bool is_stopped() const;

    /** True if the calling thread is one this server runs its callbacks on.  */
    [[nodiscard]] bool running_in_callback_thread() const;

    /** @brief The error that stopped the listener, or @c nullptr if it stopped cleanly.
     *
     * Set before OnStoppedListening() fires and cleared by the next start(), so it is valid to
     * query from inside that callback and at any point afterwards until the server is restarted.
     */
    [[nodiscard]] std::exception_ptr last_error() const;

    size_t clients_count() const;
    ptr session(session_id_t id);
    std::map<session_id_t, ptr> sessions() const;

    void write(session_id_t id, std::string_view data);
    void write(session_id_t id, const void* data, size_t size);
    void write(session_id_t id, sg::shared_c_buffer<std::byte> buffer);

    void disconnect(session_id_t id);
    void disconnect_all();

  private:
    std::atomic<bool> m_running {false};

    mutable std::shared_mutex m_mutex;
    std::map<session_id_t, ptr> m_sessions;
    std::atomic<size_t> m_active_sessions{0};
    std::atomic<size_t> m_last_id{0};

    std::vector<end_point> m_endpoints;
    std::shared_ptr<sg::net::asio_io_pool> m_context;

    //m_acceptors are kept for use by set_keepalive/set_timeout
    std::vector<std::shared_ptr<boost::asio::ip::tcp::acceptor>> m_acceptors;
    /* One retry timer per acceptor, in the same order as m_acceptors and sharing that acceptor's
     * strand. Used by listener() to back off after a transient accept() failure. */
    std::vector<std::shared_ptr<boost::asio::steady_timer>> m_accept_retry_timers;
    std::atomic<size_t> m_acceptors_running_count{0};

    CallBacks m_callbacks;

    mutable std::mutex m_error_mutex;
    std::exception_ptr m_last_error;

    std::atomic<bool> m_stop_in_operation;

    dp::thread_pool<> m_pool{1};
    options_t m_options;

    std::atomic<std::thread::id> m_callback_thread_id{};
    std::jthread m_stopping_thread; // declared last so it's joined first

    boost::asio::awaitable<void> listener(std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor,
                                          std::shared_ptr<boost::asio::steady_timer> retry_timer);

    void bind_acceptors();
    void on_io_pool_stopped(asio_io_pool&);

    /* Records the reason the listener is stopping. First writer wins: a fatal error on one
     * acceptor stops the others, and their resulting errors must not mask the original cause. */
    void record_error(std::exception_ptr ex);

    void inform_user_of_data(session_id_t id, const std::byte* data, size_t size);
    void on_session_stopped(session_id_t id,  std::exception_ptr ex);

};

}

