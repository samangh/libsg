#pragma once

#include "net.h"
#include "tcp_native.h"

#include "sg/buffer.h"
#include "sg/callback.h"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
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
         * This is useful if you want to pass the native handle to another library for reading,etc.*/
        bool dont_read {false};

        keepalive_t keepalive{};
        unsigned timeout_msec{5000};
        int recv_buffer_size{0}; // 0 = use default OS value
        int send_buffer_size{0}; // 0 = use default OS value
    };

    /** Suspends the session's reader for as long as it is alive.
     *
     * One is handed to every @c OnDataAvailable call. While it exists the session issues no
     * further reads, which is what makes it safe for the callback to work straight out of the
     * reader's own buffer, and what applies back-pressure to the peer when the consumer cannot
     * keep up. Move it somewhere longer-lived to keep the read paused past the callback's return.
     *
     * Resumption happens in the destructor, so every way out counts: returning, throwing, or
     * having the handler it was moved into destroyed without ever running. Releasing twice or
     * never is safe -- closing the session releases whatever is still outstanding -- but note
     * that a lease which is never dropped keeps the session, and hence a tcp_server's shutdown,
     * waiting. Releasing one after its session has stopped is a no-op, so a lease that outlives
     * the connection costs nothing.
     *
     * Move-only. May be released from any thread. */
    class read_lease {
      public:
        read_lease() noexcept = default;
        read_lease(read_lease&&) noexcept = default;
        read_lease& operator=(read_lease&&) noexcept;
        read_lease(const read_lease&) = delete;
        read_lease& operator=(const read_lease&) = delete;
        ~read_lease();

        /** Resumes the reader. Idempotent, and a no-op on a moved-from lease. */
        void release() noexcept;

        [[nodiscard]] explicit operator bool() const noexcept { return m_session != nullptr; }

      private:
        friend class tcp_session;
        explicit read_lease(std::shared_ptr<tcp_session> session) noexcept
            : m_session(std::move(session)) {}

        std::shared_ptr<tcp_session> m_session;
    };

    struct Callbacks {
        CREATE_CALLBACK(OnConnected, void(tcp_session&))
        CREATE_CALLBACK(OnDisconnected, void(tcp_session&, std::exception_ptr))
        CREATE_CALLBACK(OnDataAvailable, void(tcp_session&, const std::byte*, size_t, read_lease))

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

    [[nodiscard]] bool running_in_io_thread() const;
  private:
    boost::asio::ip::tcp::socket m_socket;

    /* Per-session strand. The socket is not thread-safe, so the reader, writer and close handlers
     * (which all touch m_socket) must not run concurrently. Routing all three through this strand
     * serialises them. It does NOT serialise the underlying I/O. */
    boost::asio::strand<boost::asio::ip::tcp::socket::executor_type> m_strand;

    /* Keeps the io_context this session runs on from stopping while the session is unfinished.
     *
     * The pool is deliberately guardless -- a tcp_server's io pool has to stop itself once the
     * acceptors and the last session are gone -- and a guardless context stops the instant its work
     * count reaches zero. That count is not a proxy for "this session has nothing left to do":
     * there are windows in which a session has no operation in flight and yet still owes work to
     * m_strand. The narrowest, and the one that bites, is inside close(), between the transition to
     * `stopping` and the dispatch of close_impl(): if the reader finishes in that window it takes
     * the work count to zero, the context stops, and the dispatch is queued on a stopped context
     * where it is never run. close_impl() would then never fire OnDisconnected, the session would
     * never reach `stopped`, and tcp_server::teardown() would wait on it forever.
     *
     * Held from construction and dropped at the end of close_impl(), i.e. for exactly as long as
     * anything may still have to be posted to m_strand on this session's behalf. */
    boost::asio::executor_work_guard<boost::asio::ip::tcp::socket::executor_type> m_io_work;

    options_t m_options;
    Callbacks m_callbacks;

    /* The reader's receive buffer. A session member rather than a local of reader(), because a
     * read_lease hands a pointer into it to the callback while keeping only the *session* alive:
     * close_impl() can release the gate and let the reader coroutine exit -- and its frame be
     * destroyed -- with a lease still outstanding. Written only by reader(); read by a lease
     * holder, which by construction cannot overlap with a read. */
    sg::unique_buffer<std::byte> m_read_buffer;

    /* The read gate. reader() parks here while a lease is outstanding; releasing the lease, and
     * close_impl(), let it continue.
     *
     * m_read_released is the thing the reader actually tests, and is set by whichever thread drops
     * the lease. Cancelling the timer is only the nudge that wakes a reader which has already
     * parked, and so may safely be lost; the latch is what stops a release that lands *before* the
     * reader parks from leaving it waiting for a wakeup that has already happened.
     *
     * The timer itself is not thread-safe and is touched only from m_strand. */
    boost::asio::steady_timer m_read_gate;
    std::atomic<bool> m_read_released{true};

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

    /* Lets a parked reader continue. Must be called on m_strand. */
    void release_read_gate();

    /* Raw socket-option work. NOT thread-safe with respect to other socket access — callers must
     * either be running on m_strand or be in a phase where no other thread can touch m_socket
     * (e.g. start(), before the reader/writer coroutines are spawned). */
    void apply_keepalive_unsafe(keepalive_t);
    void apply_timeout_unsafe(unsigned timeoutMSec);

    boost::asio::awaitable<void> reader();
    boost::asio::awaitable<void> writer();
};

}
