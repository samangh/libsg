#pragma once

#include "net.h"
#include "tcp_native.h"

#include "sg/buffer.h"
#include "sg/callback.h"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>
#include <memory>

namespace sg::net {

class SG_NET_EXPORT tcp_session : public std::enable_shared_from_this<tcp_session> {
    struct private_tag { explicit private_tag() = default; };

  public:
    struct options_t {
        // needed to get around clang bug https://github.com/llvm/llvm-project/issues/36032
        options_t() {};

        /* if set to true, the `on_data_available_cb_t` will be called when there is data available,
         * but the data won't have been read from the socket. You'll have to manually read it from
         * the native handle.
         *
         * This is useful if you want to pass the native handle to another library for reading,
         * etc.*/
        bool dont_read {false};

        keepalive_t keepalive{};
        unsigned timeout_msec{5000}; // 0 = don't timeout

        /* Time out on connection attempt.
         *
         * Each OS has a global maximum connection timeout. The actual time out will be the shorter
         * of `connection_timeout_msec` and the OS global default.
         *
         *   - `tcp_syn_retries` on Linux, which by dfault is ~180 seconds
         *   - `MaxSynRetransmissions` on Windows, which by default is ~21 seconds */
        unsigned connection_timeout_msec{10000}; // 0 = let OS do connection timeout
        int recv_buffer_size{0};                 // 0 = use default OS value
        int send_buffer_size{0};                 // 0 = use default OS value
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

    static std::shared_ptr<tcp_session> create(boost::asio::io_context& context,
                                               boost::asio::ip::tcp::socket socket,
                                               Callbacks callbacks,
                                               options_t options);

    tcp_session(private_tag, boost::asio::io_context& context,
                boost::asio::ip::tcp::socket socket, Callbacks cb, options_t options);
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
        /* he if is needed — being on the strand doesn't mean the context is running.
         * io_context::stop() sets the stopped flag immediately, but a worker already executing a
         * handler runs to completion.*/
        if (!running_in_io_thread())
            throw_if_io_context_stopped();

        auto fut = boost::asio::dispatch(m_strand, boost::asio::use_future(func));
        return fut.get();
    }

    [[nodiscard]] bool running_in_io_thread() const;
  private:
    boost::asio::ip::tcp::socket m_socket;

    /* Per-session strand. The socket is not thread-safe, so the reader, writer and close handlers
     * (which all touch m_socket) must not run concurrently. Routing all three through this strand
     * serialises them. It does NOT serialise the underlying I/O. */
    boost::asio::strand<boost::asio::ip::tcp::socket::executor_type> m_strand;

    /* The io_context that m_strand runs on. A stopped io_context runs no handlers, so anything
     * handed to m_strand while it is stopped never happens; the calls that would otherwise wait
     * for it check stopped() to avoid blocking forever.
     *
     * Note that a context that has not been run yet is not stopped: work handed to it now is
     * processed once it is run (see asio_io_pool::run()). */
    boost::asio::io_context& m_io_context;

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

    void close();
    void close_impl();

    void throw_if_io_context_stopped() const;

    /* Raw socket-option work. NOT thread-safe with respect to other socket access — callers must
     * either be running on m_strand or be in a phase where no other thread can touch m_socket
     * (e.g. start(), before the reader/writer coroutines are spawned). */
    void apply_keepalive_unsafe(keepalive_t);
    void apply_timeout_unsafe(unsigned timeoutMSec);

    boost::asio::awaitable<void> reader();
    boost::asio::awaitable<void> writer();
};

}
