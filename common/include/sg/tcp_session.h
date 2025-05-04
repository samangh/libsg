#pragma once

#include "net.h"
#include "buffer.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <optional>
#include <thread_pool/thread_safe_queue.h>

namespace sg::net {

class SG_COMMON_EXPORT tcp_session :  public std::enable_shared_from_this<tcp_session>{
  public:
    typedef std::function<void(const std::byte*, size_t)> on_data_available_cb_t;
    typedef std::function<void(std::optional<std::exception>)> on_disconnected_cb_t;

    tcp_session(boost::asio::ip::tcp::socket socket, on_data_available_cb_t onReadCb, on_disconnected_cb_t onErrorCb);
    ~tcp_session();

    void start();
    void stop_async();

    sg::net::end_point local_endpoint();
    sg::net::end_point remote_endpoint();

    void write(sg::shared_c_buffer<std::byte> msg);
    void write(std::string_view msg);
  private:    
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::steady_timer m_timer;

    on_data_available_cb_t m_on_data_cb;
    on_disconnected_cb_t  m_on_disconnected_cb;

    std::mutex m_mutex;
    dp::thread_safe_queue<sg::shared_c_buffer<std::byte>> m_write_msgs;

    std::atomic<bool> m_disconnected_cb_called {false};
    std::atomic<bool> m_stop_requested{false};

    void close(std::optional<std::exception> ex);

    boost::asio::awaitable<void> reader();
    boost::asio::awaitable<void> writer();
};

}
