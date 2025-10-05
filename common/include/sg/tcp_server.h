#pragma once

#include <sg/export/common.h>

#include "net.h"
#include "buffer.h"
#include "notifiable_background_worker.h"
#include "jthread.h"
#include "tcp_session.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <map>
#include <optional>

namespace sg::net {


class SG_COMMON_EXPORT tcp_server {
  public:
    typedef size_t session_id_t;
    typedef std::shared_ptr<tcp_session> ptr;

    typedef std::function<void(tcp_server&)> started_listening_cb_t;
    typedef std::function<void(tcp_server&)> stopped_listening_cb_t;
    typedef std::function<void(tcp_server&, session_id_t)> session_created_cb_t;
    typedef std::function<void(tcp_server&, session_id_t, const std::byte*, size_t)> session_data_available_cb_t;
    typedef std::function<void(tcp_server&, session_id_t, std::optional<std::exception>)> session_disconnected_cb_t;

    ~tcp_server() noexcept(false);

    void start(std::vector<end_point> endpoints,
               started_listening_cb_t onStartListening,
               stopped_listening_cb_t onStopListeniing,
               session_created_cb_t onNewSession,
               session_data_available_cb_t onDataAvailCb,
               session_disconnected_cb_t onDisconnCb) noexcept(false);

    void stop_async();
    void future_get_once() noexcept(false);
    bool is_stopped() const;

    size_t clients_count() const;

    void write(session_id_t id, const void* data, size_t size);
    void write(session_id_t id, sg::shared_c_buffer<std::byte> buffer);

    void disconnect(session_id_t id);
    void disconnect_all();

    ptr session(session_id_t id);

  private:
    std::unique_ptr<notifiable_background_worker> m_worker;

    mutable std::shared_mutex m_mutex;
    std::map<session_id_t, ptr> m_sessions;
    std::atomic<size_t> m_last_id;

    std::vector<end_point> m_endpoints;
    std::promise<void> m_promise_started_listening;
    boost::asio::io_context m_io_context_ptr;

    session_created_cb_t m_new_session_cb;
    session_data_available_cb_t m_on_data_read_user_cb;
    session_disconnected_cb_t m_on_disconnect_user_cb;
    started_listening_cb_t m_on_started_listening_cb;
    stopped_listening_cb_t m_on_stopped_listening_cb;

    std::atomic<bool> m_stop_in_operation;
    std::jthread m_stopping_thread;

    boost::asio::awaitable<void> listener(boost::asio::ip::tcp::acceptor acceptor);

    void on_worker_start(notifiable_background_worker*);
    void on_worker_stop(notifiable_background_worker*);
    void on_worker_tick(notifiable_background_worker*);

    void inform_user_of_data(session_id_t id, const std::byte* data, size_t size);
    void on_session_stopped(session_id_t id, std::optional<std::exception> ex);

};

}

