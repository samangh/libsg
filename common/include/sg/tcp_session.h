#pragma once

#include "buffer.h"
#include "net.h"
#include "callback.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <thread_pool/thread_safe_queue.h>

#include <optional>
#include <future>
#include <condition_variable>

namespace sg::net {

class SG_COMMON_EXPORT tcp_session {
  public:
    CREATE_CALLBACK(on_data_available_cb_t, void, const std::byte*, size_t)
    CREATE_CALLBACK(on_disconnected_cb_t, void, std::optional<std::exception>)

    tcp_session(boost::asio::ip::tcp::socket socket, on_data_available_cb_t onReadCb, on_disconnected_cb_t onErrorCb);
    virtual ~tcp_session();

    void start();
    void stop_async();
    void wait_until_stopped() const;
    bool is_connected() const;

    end_point local_endpoint();
    end_point remote_endpoint();

    void write(sg::shared_c_buffer<std::byte> msg);
    void write(std::string_view msg);
    void write(const void* data, size_t size);

    void set_keepalive(bool enableKeepAlive, unsigned idleSec = 60, unsigned intervalSec = 5,
                       unsigned count = 5);
    void set_timeout(unsigned timeoutMSec = 5000);

  private:
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::steady_timer m_timer;

    on_data_available_cb_t m_on_data_cb;
    on_disconnected_cb_t  m_on_disconnected_cb;

    dp::thread_safe_queue<sg::shared_c_buffer<std::byte>> m_write_msgs{};

    std::atomic<bool> m_disconnected_cb_called {false};
    std::atomic<bool> m_stop_requested{false};
    std::atomic<bool> m_stopped {true};

    std::atomic<bool> m_reader_running;
    std::atomic<bool> m_writer_running;

    std::mutex m_exception_mutex;
    std::string m_exception_msg;

    void close();

    boost::asio::awaitable<void> reader();
    boost::asio::awaitable<void> writer();
};

}
