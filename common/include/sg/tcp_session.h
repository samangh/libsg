#pragma once

#include "buffer.h"
#include "callback.h"
#include "net.h"
#include "tcp_native.h"

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
    struct options_t {
        // needed to get around clang bug https://github.com/llvm/llvm-project/issues/36032
        options_t() {};

        /* if set to true, the `on_data_available_cb_t` will be called when there is data available,
         * but the data won't have been read from the socket. You'll have to manually read it from
         * teh native handle.
         *
         * This is useful if you want to pass the native handle to another library for reading,etc.*/
        bool dont_read {false};
    };

    CREATE_CALLBACK(on_data_available_cb_t, void, tcp_session&, const std::byte*, size_t)
    CREATE_CALLBACK(on_disconnected_cb_t, void, tcp_session&, std::optional<std::exception>)

    tcp_session(boost::asio::ip::tcp::socket socket, on_data_available_cb_t onReadCb,
                on_disconnected_cb_t onErrorCb, options_t options=options_t());
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

    native::socket_t native_handle();

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
    options_t m_options;

    std::mutex m_exception_mutex;
    std::string m_exception_msg;

    void close();

    boost::asio::awaitable<void> reader();
    boost::asio::awaitable<void> writer();
};

}
