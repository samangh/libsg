#pragma once
#include <sg/export/common.h>

#include "asio_io_pool.h"
#include "buffer.h"
#include "jthread.h"
#include "net.h"
#include "notifiable_background_worker.h"
#include "tcp_session.h"
#include "callback.h"

#include <thread_pool/thread_pool.h>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <map>
#include <optional>

namespace sg::net {


class SG_COMMON_EXPORT tcp_server {
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

    CREATE_CALLBACK(started_listening_cb_t, void, tcp_server&)
    CREATE_CALLBACK(stopped_listening_cb_t, void, tcp_server&)
    CREATE_CALLBACK(session_created_cb_t, void, tcp_server&, session_id_t)
    CREATE_CALLBACK(session_data_available_cb_t, void, tcp_server&, session_id_t, const std::byte*, size_t)
    CREATE_CALLBACK(session_disconnected_cb_t, void, tcp_server&, session_id_t, std::optional<std::exception>)

    struct CallBacks {
        started_listening_cb_t OnStartedListening;
        stopped_listening_cb_t OnStoppedListening;
        session_created_cb_t OnSessionCreated;
        session_data_available_cb_t OnSessionDataAvailable;
        session_disconnected_cb_t OnDisconnected;
    };

    struct options_t {
        options_t() {}; // work around bug https://github.com/llvm/llvm-project/issues/36032
        tcp_session::options_t session_options{};
        size_t no_threads{1};
    };

    ~tcp_server() noexcept(false);

    void start(std::vector<end_point> endpoints, CallBacks callbacks,
               options_t options = options_t()) noexcept(false);

    void stop_async();
    void future_get_once() noexcept(false);
    bool is_stopped() const;

    size_t clients_count() const;
    ptr session(session_id_t id);
    std::map<session_id_t, ptr> sessions() const;

    void write(session_id_t id, std::string_view data);
    void write(session_id_t id, const void* data, size_t size);
    void write(session_id_t id, sg::shared_c_buffer<std::byte> buffer);

    void disconnect(session_id_t id);
    void disconnect_all();

    void set_keepalive(bool enableKeepAlive, unsigned idleSec = 60, unsigned intervalSec = 5,
                       unsigned count = 5);
    void set_timeout(unsigned timeoutMSec = 5000);

  private:
    mutable std::shared_mutex m_mutex;
    std::map<session_id_t, ptr> m_sessions;
    std::atomic<size_t> m_last_id;

    std::vector<end_point> m_endpoints;
    std::promise<void> m_promise_started_listening;
    std::shared_ptr<sg::net::asio_io_pool> m_context;

    //m_acceptors are kept for use by set_keepalive/set_timeout
    std::vector<std::shared_ptr<boost::asio::ip::tcp::acceptor>> m_acceptors;
    std::atomic<size_t> m_acceptors_running_count{0};
    std::atomic<bool> m_acceptors_stopped{false};

    CallBacks m_callbacks;

    std::atomic<bool> m_stop_in_operation;
    std::jthread m_stopping_thread;

    dp::thread_pool<> m_pool{1};
    options_t m_options;

    boost::asio::awaitable<void> listener(std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor);

    void start_listening();
    void on_io_pool_stopped(asio_io_pool&);

    void inform_user_of_data(session_id_t id, const std::byte* data, size_t size);
    void on_session_stopped(session_id_t id, std::optional<std::exception> ex);

};

}

