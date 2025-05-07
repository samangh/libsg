#pragma once

#include "sg/export/common.h"

#include "net.h"
#include "buffer.h"
#include "notifiable_background_worker.h"
#include "tcp_context.h"
#include "tcp_session.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <map>

namespace sg::net {

class SG_COMMON_EXPORT tcp_server {
  public:
    typedef size_t session_id_t;
    typedef std::shared_ptr<tcp_session> ptr;

    typedef std::function<void(tcp_server&)> started_listening_cb_t;
    typedef std::function<void(tcp_server&, std::exception_ptr)> stopped_listening_cb_t;
    typedef std::function<void(tcp_server&, session_id_t)> session_created_cb_t;
    typedef std::function<void(tcp_server&, session_id_t, const std::byte*, size_t)> session_data_available_cb_t;
    typedef std::function<void(tcp_server&, session_id_t, std::exception_ptr)> session_disconnected_cb_t;

    tcp_server();
    tcp_server(std::shared_ptr<tcp_context> context);
    ~tcp_server() noexcept(false);

    void start(std::vector<end_point> endpoints,
               started_listening_cb_t onStartListening,
               stopped_listening_cb_t onStopListeniing,
               session_created_cb_t onNewSession,
               session_data_available_cb_t onDataAvailCb,
               session_disconnected_cb_t onDisconnCb) noexcept(false);

    void stop_async();
    void wait_until_stopped() const;
    void get_future_once() const;

    size_t clients_count() const;

    void write(session_id_t id, const void* data, size_t size);
    void write(session_id_t id, sg::shared_c_buffer<std::byte> buffer);

    void disconnect(session_id_t id);
    void disconnect_all();

    ptr session(session_id_t id);

  private:
    std::shared_ptr<tcp_context> m_context;

    mutable std::shared_mutex m_mutex;
    std::map<session_id_t, ptr> m_sessions;
    std::atomic<size_t> m_last_id;

    std::vector<end_point> m_endpoints;
    std::promise<void> m_promise_started_listening;

    session_created_cb_t m_new_session_cb;
    session_data_available_cb_t m_on_data_read_user_cb;
    session_disconnected_cb_t m_on_disconnect_user_cb;
    started_listening_cb_t m_on_started_listening_cb;
    stopped_listening_cb_t m_on_stopped_listening_cb;

    std::atomic<bool> m_stop_in_operation;
    std::unique_ptr<sg::notifiable_background_worker> m_stopping_thread;

    std::atomic<bool> m_is_running{false};

    std::mutex  m_listening_exception_mutex;
    std::exception_ptr m_listening_exception;

    boost::asio::awaitable<void> listener(boost::asio::ip::tcp::acceptor* acceptor);

    std::vector<std::unique_ptr<boost::asio::ip::tcp::acceptor>> m_acceptors;

    void listen_on_endpoints();
    void on_stop();

    void inform_user_of_data(session_id_t id, const std::byte* data, size_t size);
    void on_session_stopped(session_id_t id, std::exception_ptr ex);

};

}

