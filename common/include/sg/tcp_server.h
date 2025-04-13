#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "net.h"
#include "buffer.h"
#include "notifiable_background_worker.h"

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

class tcp_session :  public std::enable_shared_from_this<tcp_session>{
  public:
    typedef sg::shared_c_buffer<std::byte> buffer_t;
    typedef std::function<void(const std::byte*, size_t)> on_data_available_cb_t;
    typedef std::function<void(std::optional<std::exception>)> on_disconnected_cb_t;

    tcp_session(boost::asio::ip::tcp::socket socket, on_data_available_cb_t onReadCb, on_disconnected_cb_t onErrorCb)
        : m_socket(std::move(socket)),
          m_timer(m_socket.get_executor()),
          m_on_data_cb(std::move(onReadCb)),
          m_on_disconnected_cb(std::move(onErrorCb))
    {
        m_timer.expires_at(std::chrono::steady_clock::time_point::max());

        boost::asio::socket_base::keep_alive option(true);
        m_socket.set_option(option);
    }

    ~tcp_session(){
        if (!m_disconnected_cb_called.exchange(true) && m_on_disconnected_cb)
            m_on_disconnected_cb({});
    }

    void start() {
        co_spawn(m_socket.get_executor(),
                 [self = shared_from_this()]{ return self->reader(); },
                 boost::asio::detached);

        co_spawn(m_socket.get_executor(),
                 [self = shared_from_this()]{ return self->writer(); },
                 boost::asio::detached);
    }

    void write(sg::shared_c_buffer<std::byte> msg)
    {
        write_msgs_.push_back(std::move(msg));
        m_timer.cancel_one();
    }

    void stop(){
        stop({});
    }
  private:
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::steady_timer m_timer;

    on_data_available_cb_t m_on_data_cb;
    on_disconnected_cb_t  m_on_disconnected_cb;

    std::mutex m_mutex;
    dp::thread_safe_queue<buffer_t> write_msgs_;

    std::atomic<bool> m_disconnected_cb_called {false};

    void stop(std::optional<std::exception> ex){
        m_disconnected_cb_called.store(true);
        if (m_on_disconnected_cb)
            m_on_disconnected_cb(ex);

        m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_type::shutdown_both);
        m_socket.close();
        m_timer.cancel();
    }

    boost::asio::awaitable<void> reader()
    {
        try
        {
            boost::asio::socket_base::receive_buffer_size option;
            m_socket.get_option(option);
            int size = option.value();

            auto data= std::make_unique<std::byte[]>(size);
            while (true)
            {
                std::size_t n = co_await m_socket.async_read_some(boost::asio::buffer(data.get(), size), boost::asio::use_awaitable);
                if (m_on_data_cb)
                    m_on_data_cb(data.get(), n);
            }
        }
        catch (const std::exception& ex)
        {
            stop(ex);
        }
    }

    boost::asio::awaitable<void> writer() {
        try {
            while (m_socket.is_open())
                if (write_msgs_.empty()) {
                    boost::system::error_code ec;
                    co_await m_timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                } else {
                    auto front = write_msgs_.pop_front();
                    co_await boost::asio::async_write(
                        m_socket, boost::asio::buffer(front->get(), front->size()), boost::asio::use_awaitable);
                }

        } catch (const std::exception& ex) {
            stop(ex);
        }
    }
};

class tcp_server {
  public:
    typedef size_t session_id_t;

    typedef std::function<void(tcp_server&)> started_listening_cb_t;
    typedef std::function<void(tcp_server&)> stopped_listening_cb_t;
    typedef std::function<void(tcp_server&, session_id_t)> new_session_cb_t;
    typedef std::function<void(tcp_server&, session_id_t, const std::byte*, size_t)> session_data_available_cb_t;
    typedef std::function<void(tcp_server&, session_id_t, std::optional<std::exception>)> session_disconnected_cb_t;

    ~tcp_server() {        
        disconnect_all();
        stop_async();
        m_worker->wait_for_stop();
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

    void stop_async() {
        if (!m_io_context_ptr.stopped())
            m_io_context_ptr.stop();
    }


    void write(session_id_t id, const void* data, size_t size) {
        auto ptr = sg::make_shared_c_buffer<std::byte>(size);
        std::memcpy(ptr.get(), data, size);
        write(id, ptr);
    }

    void write(session_id_t id, tcp_session::buffer_t buffer)    {
        std::shared_lock lock(m_mutex);
        m_sessions.at(id)->write(buffer);
    }

    void disconnect(session_id_t id) {
        ptr sess;
        {
            std::shared_lock lock(m_mutex);
            sess =m_sessions.at(id);
        }

        sess->stop();
    }

    void disconnect_all() {
        std::vector<ptr> vec;
        {
            std::shared_lock lock(m_mutex);
            for (auto [_, sess]: m_sessions)
                vec.push_back(sess);
        }

        for (auto& sess: vec)
            sess->stop();
    }

  private:
    typedef std::shared_ptr<tcp_session> ptr;

    std::unique_ptr<notifiable_background_worker> m_worker;

    std::shared_mutex m_mutex;
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
