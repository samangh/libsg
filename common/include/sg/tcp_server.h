#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "net.h"
#include "buffer.h"
#include "notifiable_background_worker.h"
#include "jthread.h"
#include "tcp_session.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/write.hpp>

#include <map>
#include <optional>
#include <thread_pool/thread_safe_queue.h>

namespace sg::net {


class tcp_server {
  public:
    typedef size_t session_id_t;
    typedef std::shared_ptr<tcp_session> ptr;

    typedef std::function<void(tcp_server&)> started_listening_cb_t;
    typedef std::function<void(tcp_server&)> stopped_listening_cb_t;
    typedef std::function<void(tcp_server&, session_id_t)> new_session_cb_t;
    typedef std::function<void(tcp_server&, session_id_t, const std::byte*, size_t)> session_data_available_cb_t;
    typedef std::function<void(tcp_server&, session_id_t, std::optional<std::exception>)> session_disconnected_cb_t;

    ~tcp_server() noexcept(false) {
        if (m_worker && m_worker->is_running())
        {
            stop_async();
            m_worker->future_get_once();
        }
    }

    void stop_async() {
        /* if stop thread not started, start */
        if (m_stop_in_operation.exchange(true))
            return;

        m_stopping_thread = std::jthread([this]() {
            disconnect_all();

            /* wait untill all clients disconnected and all callbacks called, etc */
            while (clients_count() != 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (!m_io_context_ptr.stopped())
                m_io_context_ptr.stop();
            m_worker->wait_for_stop();
        });
    }

    void start(std::vector<end_point> endpoints,
               started_listening_cb_t onStartListening,
               stopped_listening_cb_t onStopListeniing,
               new_session_cb_t onNewSession,
               session_data_available_cb_t onDataAvailCb,
               session_disconnected_cb_t onDisconnCb) noexcept(false) {
        if (m_worker && m_worker->is_running())
            throw std::runtime_error("tcp_server is already running");

        m_on_started_listening_cb = onStartListening;
        m_on_stopped_listening_cb = onStopListeniing;
        m_new_session_cb = std::move(onNewSession),
        m_on_data_read_user_cb = std::move(onDataAvailCb);
        m_on_disconnect_user_cb = std::move(onDisconnCb);

        m_stop_in_operation.store(false);

        m_endpoints = endpoints;
        m_last_id = 0;

        m_promise_started_listening = std::promise<void>();

        auto serverThreadTask = std::bind(&tcp_server::server_thread_task, &*this, std::placeholders::_1);
        auto serverSetupTask = std::bind(&tcp_server::on_worker_start, &*this, std::placeholders::_1);
        auto serverStopTask = std::bind(&tcp_server::on_worker_stop, &*this, std::placeholders::_1);

        m_worker = std::make_unique<notifiable_background_worker>(
            std::chrono::seconds(1), serverThreadTask, serverSetupTask, serverStopTask);
        m_worker->start();
    }

    void future_get_once() noexcept(false) {
        m_worker->future_get_once();
    }

    bool is_stopped() const {
        if (m_worker)
            return !(m_worker->is_running());

        return true;
    }

    size_t clients_count() const {
        std::shared_lock lock(m_mutex);
        return m_sessions.size();
    }

    void write(session_id_t id, const void* data, size_t size) {
        if (m_stop_in_operation.load(std::memory_order::acquire))
            throw std::runtime_error("can't write as a stop has been requested");

        auto ptr = sg::make_shared_c_buffer<std::byte>(size);
        std::memcpy(ptr.get(), data, size);
        write(id, ptr);
    }

    void write(session_id_t id, sg::shared_c_buffer<std::byte> buffer)    {
        std::shared_lock lock(m_mutex);
        m_sessions.at(id)->write(buffer);
    }

    void disconnect(session_id_t id) {
        std::shared_lock lock(m_mutex);
        m_sessions.at(id)->stop_async();
    }

    void disconnect_all() {
        std::shared_lock lock(m_mutex);
        for (auto [_, sess]: m_sessions)
            sess->stop_async();
    }

    ptr session(session_id_t id){
        std::shared_lock lock(m_mutex);
        return m_sessions.at(id);
    }

  private:
    std::unique_ptr<notifiable_background_worker> m_worker;

    mutable std::shared_mutex m_mutex;
    std::map<session_id_t, ptr> m_sessions;
    std::atomic<size_t> m_last_id;

    std::vector<end_point> m_endpoints;
    std::promise<void> m_promise_started_listening;
    boost::asio::io_context m_io_context_ptr;

    new_session_cb_t m_new_session_cb;
    session_data_available_cb_t m_on_data_read_user_cb;
    session_disconnected_cb_t m_on_disconnect_user_cb;
    started_listening_cb_t m_on_started_listening_cb;
    stopped_listening_cb_t m_on_stopped_listening_cb;

    std::atomic<bool> m_stop_in_operation;
    std::jthread m_stopping_thread;

    boost::asio::awaitable<void> listener(boost::asio::ip::tcp::acceptor acceptor) {
        while (true) {
            auto id = m_last_id++;

            auto onSessionDisconnected = [this, id](std::optional<std::exception> ex) {
                on_session_stopped(id, ex);
            };
            auto onData = [this, id](const std::byte* data, size_t size) {
                inform_user_of_data(id, data, size);
            };

            auto sess = std::make_shared<tcp_session>(
                co_await acceptor.async_accept(boost::asio::use_awaitable), onData, onSessionDisconnected);

            {
                std::unique_lock lock(m_mutex);
                m_sessions.emplace(id, sess);
            }

            if (m_new_session_cb)
                m_new_session_cb(*this, id);

            sess->start();
        }
    }

    void on_worker_start(notifiable_background_worker*) {
        for (auto e : m_endpoints) {
            boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address(e.ip),
                                              static_cast<boost::asio::ip::port_type>(e.port));
            boost::asio::co_spawn(
                m_io_context_ptr,
                listener(boost::asio::ip::tcp::acceptor(m_io_context_ptr, ep)),
                boost::asio::detached);
        }

        if (m_on_started_listening_cb)
            m_on_started_listening_cb(*this);
    }

    void on_worker_stop(notifiable_background_worker*) {
        if (m_on_stopped_listening_cb)
            m_on_stopped_listening_cb(*this);
    }

    void server_thread_task(notifiable_background_worker* worker) {
        m_io_context_ptr.run();
        worker->request_stop();
    }

    void inform_user_of_data(session_id_t id, const std::byte* data, size_t size) {
        if (m_on_data_read_user_cb)
            m_on_data_read_user_cb(*this, id, data, size);
    }

    void on_session_stopped(session_id_t id, std::optional<std::exception> ex)
    {
        if (m_on_disconnect_user_cb)
            m_on_disconnect_user_cb(*this, id, ex);

        std::unique_lock lock(m_mutex);
        m_sessions.erase(id);
    }

};

}

#endif // TCP_SERVER_H
