#pragma once

#include "buffer.h"
#include "callback.h"
#include "net.h"
#include "tcp_native.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace sg::net {

class SG_COMMON_EXPORT tcp_session {
  public:
    struct options_t {
        // needed to get around clang bug https://github.com/llvm/llvm-project/issues/36032
        options_t() {};

        /* if set to true, the `on_data_available_cb_t` will be called when there is data available,
         * but the data won't have been read from the socket. You'll have to manually read it from
         * the native handle.
         *
         * This is useful if you want to pass the native handle to another library for reading,etc.*/
        bool dont_read {false};

        keepalive_t keepalive{};
        unsigned timeout_msec{5000};
    };

    CREATE_CALLBACK(on_data_available_cb_t, void, tcp_session&, const std::byte*, size_t)
    CREATE_CALLBACK(on_connected_cb_t, void, tcp_session&)
    CREATE_CALLBACK(on_disconnected_cb_t, void, tcp_session&, std::exception_ptr)

    tcp_session(boost::asio::ip::tcp::socket socket, on_data_available_cb_t onReadCb,
                on_disconnected_cb_t onErrorCb, options_t options);
    virtual ~tcp_session();

    void start(on_connected_cb_t);
    void stop_async();
    void wait_until_stopped() const;
    [[nodiscard]] bool is_connected() const;

    [[nodiscard]] end_point local_endpoint() const noexcept(false);
    [[nodiscard]] end_point remote_endpoint() const noexcept(false);

    void write(sg::shared_c_buffer<std::byte> msg);
    void write(std::string_view msg);
    void write(const void* data, size_t size);

    void set_keepalive(keepalive_t);
    void set_timeout(unsigned timeoutMSec = 5000);

    native::socket_t native_handle();

  private:
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::steady_timer m_timer;

    on_data_available_cb_t m_on_data_cb;
    on_disconnected_cb_t  m_on_disconnected_cb;

    std::mutex m_write_mutex;
    std::vector<sg::shared_c_buffer<std::byte>> m_write_msgs{};

    std::atomic<bool> m_disconnected_cb_called {false};
    std::atomic<bool> m_stop_requested{false};
    std::atomic<bool> m_stopped {true};

    // the _running vars need initialisation, as close() might be called in start()
    std::atomic<bool> m_reader_running{false};
    std::atomic<bool> m_writer_running{false};
    options_t m_options;

    std::mutex m_exception_mutex;
    std::exception_ptr m_exception;

    std::mutex m_sock_op_mutex;

    void close();

    boost::asio::awaitable<void> reader();
    boost::asio::awaitable<void> writer();

    boost::asio::awaitable<void> async_timeout(std::chrono::steady_clock::time_point& deadline);
};

}
