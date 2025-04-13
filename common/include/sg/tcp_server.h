#ifndef TCP_SERVER_H
#define TCP_SERVER_H

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

typedef uint16_t port_t;
typedef size_t session_id_t;

class tcp_session :  public std::enable_shared_from_this<tcp_session>{
  public:
    typedef sg::shared_c_buffer<std::byte> buffer_t;
    typedef std::function<void(const std::byte*, size_t)> on_data_available_cb_t;
    typedef std::function<void(const std::exception&)> on_error_cb_t;

    tcp_session(session_id_t id, boost::asio::ip::tcp::socket socket, on_data_available_cb_t onReadCb, on_error_cb_t onErrorCb)
        :m_id(id),
          m_socket(std::move(socket)),
          m_timer(m_socket.get_executor()),
          m_on_data_cb(std::move(onReadCb)),
          on_error_cb(std::move(onErrorCb))
    {
        m_timer.expires_at(std::chrono::steady_clock::time_point::max());

        boost::asio::socket_base::keep_alive option(true);
        m_socket.set_option(option);
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
  private:
    session_id_t m_id;
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::steady_timer m_timer;

    on_data_available_cb_t m_on_data_cb;
    on_error_cb_t  on_error_cb;

    std::mutex m_mutex;
    dp::thread_safe_queue<buffer_t> write_msgs_;

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
            if (on_error_cb)
                on_error_cb(ex);
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
            if (on_error_cb)
                on_error_cb(ex);
        }
    }
};

class tcp_server {
  public:
    typedef std::function<void(session_id_t, const std::byte*, size_t)> session_data_available_cb_t;
    typedef std::function<void(session_id_t, std::optional<std::exception>)> session_disconnected_cb_t;

    void start(std::vector<port_t> ports, session_data_available_cb_t onDataAvailCb,  session_disconnected_cb_t onDisconnCb) {
        if (m_worker && m_worker->is_running())
            throw std::runtime_error("tcp_server is already running");

        m_on_data_read_user_cb = std::move(onDataAvailCb);
        m_on_disconnect_user_cb = std::move(onDisconnCb);

        m_ports = ports;

        auto serverThreadTask = std::bind(&tcp_server::server_thread_task, &*this, std::placeholders::_1);
        m_worker = std::make_unique<notifiable_background_worker>(
            std::chrono::seconds(1), serverThreadTask, nullptr, nullptr);
        m_worker->start();
    }
    void stop() noexcept(false) {
        if (m_worker->is_running()) {
            m_io_context_ptr.load()->stop();
            m_worker->stop_requested();
            m_worker->wait_for_stop();
        }

        //may throw!
        m_worker->future_get_once();
    }

    void write(session_id_t id, tcp_session::buffer_t buffer)    {
        std::shared_lock lock(m_mutex);
        m_sessions.at(id)->write(buffer);
    }

    void disconnect(session_id_t id) {
        drop_connection(id, {});
    }
  private:
    typedef std::shared_ptr<tcp_session> ptr;

    std::vector<port_t> m_ports;
    std::unique_ptr<notifiable_background_worker> m_worker;

    std::atomic<size_t> m_last_id{0};

    std::shared_mutex m_mutex;
    std::map<session_id_t, ptr> m_sessions;
    std::atomic<boost::asio::io_context*> m_io_context_ptr;

    session_data_available_cb_t m_on_data_read_user_cb;
    session_disconnected_cb_t m_on_disconnect_user_cb;

    boost::asio::awaitable<void> listener(boost::asio::ip::tcp::acceptor acceptor) {

        while (true) {
            auto id = m_last_id++;

            auto onError = [this, id](const std::exception& ex) {
                drop_connection(id, ex);
            };
            auto onData = [this, id](const std::byte* data, size_t size) {
                inform_user_of_data(id, data, size);
            };

            auto sess = std::make_shared<tcp_session>(
                id, co_await acceptor.async_accept(boost::asio::use_awaitable), onData, onError);

            {
                std::unique_lock lock(m_mutex);
                m_sessions.emplace(id, sess);
            }
            sess->start();
        }
    }

    void server_thread_task(notifiable_background_worker* worker) {
        boost::asio::io_context m_io_context;
        m_io_context_ptr = &m_io_context;

        for (auto port : m_ports)
            boost::asio::co_spawn(
                m_io_context,
                listener(boost::asio::ip::tcp::acceptor(
                    m_io_context,
                    {boost::asio::ip::tcp::v4(), static_cast<boost::asio::ip::port_type>(port)})),
                boost::asio::detached);
        m_io_context.run();

        /* stop after one run */
        worker->request_stop();
    }

    void inform_user_of_data(session_id_t id, const std::byte* data, size_t size) {
        if (m_on_data_read_user_cb)
            m_on_data_read_user_cb(id, data, size);
    }

    void drop_connection(session_id_t id, std::optional<std::exception> ex)
    {
        if (m_on_disconnect_user_cb)
            m_on_disconnect_user_cb(id, ex);

        std::unique_lock lock(m_mutex);
        m_sessions.erase(id);
    }


};

}

#endif // TCP_SERVER_H
