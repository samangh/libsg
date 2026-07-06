#pragma once

#include "buffer.h"
#include "callback.h"
#include "net.h"
#include "tcp_native.h"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>
#include <memory>

namespace sg::net {

class SG_COMMON_EXPORT tcp_session : public std::enable_shared_from_this<tcp_session> {
    struct private_tag { explicit private_tag() = default; };

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
        int recv_buffer_size{0}; // 0 = use default OS value
        int send_buffer_size{0}; // 0 = use default OS value
    };

    struct Callbacks {
        CREATE_CALLBACK(OnConnected, void(tcp_session&))
        CREATE_CALLBACK(OnDisconnected, void(tcp_session&, std::exception_ptr))
        CREATE_CALLBACK(OnDataAvailable, void(tcp_session&, const std::byte*, size_t))

        OnConnected onConnected;
        OnDisconnected onDisconnected;
        OnDataAvailable onDataAvailable;
    };

    // For legacy
    typedef Callbacks::OnConnected on_connected_cb_t;
    typedef Callbacks::OnDisconnected on_disconnected_cb_t;
    typedef Callbacks::OnDataAvailable on_data_available_cb_t;

    enum class state_t {running, stop_requested, stopping, stopped };

    static std::shared_ptr<tcp_session> create(boost::asio::ip::tcp::socket socket,
                                               Callbacks callbacks,
                                               options_t options);

    tcp_session(private_tag, boost::asio::ip::tcp::socket socket, Callbacks cb, options_t options);
    ~tcp_session();

    void start();
    void stop_async();
    void wait_until_stopped() const;
    [[nodiscard]] bool is_connected() const noexcept;
    [[nodiscard]] state_t state() const noexcept;

    [[nodiscard]] end_point local_endpoint() const;
    [[nodiscard]] end_point remote_endpoint() const;

    void write(sg::shared_c_buffer<std::byte> msg);
    void write(std::string_view msg);
    void write(const void* data, size_t size);

    void set_keepalive(keepalive_t);
    void set_timeout(unsigned timeoutMSec);

    /** note: native sockets should ONLY be handled in the I/O thread ((as native handles are not
     *  thread). Use @c get_executor or @c run_in_executor to achieve this. */
    [[nodiscard]] native::socket_t native_handle();

    /** returns the ASIO executor that all the I/O operations run on */
    [[nodiscard]] boost::asio::any_io_executor get_executor() const;

    /** runs the passed function in the all the I/O operations runs on */
    [[nodiscard]] auto run_in_executor(std::invocable<> auto func) {
        auto fut = boost::asio::dispatch(m_strand, boost::asio::use_future(func));
        return fut.get();
    }

  private:
    boost::asio::ip::tcp::socket m_socket;

    /* Per-session strand. The socket is not thread-safe, so the reader, writer and close handlers
     * (which all touch m_socket) must not run concurrently. Routing all three through this strand
     * serialises them. It does NOT serialise the underlying I/O. */
    boost::asio::strand<boost::asio::ip::tcp::socket::executor_type> m_strand;

    options_t m_options;
    Callbacks m_callbacks;

    end_point m_local_endpoint {};
    end_point m_remote_endpoint {};

    std::mutex m_write_mutex;
    bool m_write_scheduled{false}; //note: need to always lock m_write_mutex
    std::vector<sg::shared_c_buffer<std::byte>> m_write_msgs{};

    std::atomic<state_t> m_state{state_t::stopped};

    std::mutex m_exception_mutex;
    std::exception_ptr m_exception;

    bool m_destructor_called = false;
    void close();
    void close_impl();

    /* Raw socket-option work. NOT thread-safe with respect to other socket access — callers must
     * either be running on m_strand or be in a phase where no other thread can touch m_socket
     * (e.g. start(), before the reader/writer coroutines are spawned). */
    void apply_keepalive_unsafe(keepalive_t);
    void apply_timeout_unsafe(unsigned timeoutMSec);

    boost::asio::awaitable<void> reader();
    boost::asio::awaitable<void> writer();
};

}
