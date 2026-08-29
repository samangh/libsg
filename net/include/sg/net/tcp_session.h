#pragma once

#include "net.h"
#include "sg/buffer.h"
#include "sg/callback.h"
#include "tcp_native.h"
#include "tcp_transport.h"

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
         *   - `MaxSynRetransmissions` on Windows, which by default is ~21 seconds
         *
         * Note that the connection timeout does not apply to DNS resolving. The DNS resolve timeout
         * is set globally by the system/os.
         */
        unsigned connection_timeout_msec{10000}; // 0 = let OS do connection timeout
        int recv_buffer_size{0};                 // 0 = use default OS value
        int send_buffer_size{0};                 // 0 = use default OS value

        /* Maximum number bytes that may be pending in the write buffer before write() starts
         * refusing data.
         *
         * For TLS connections, this is counted before encryption is applied. */
        size_t write_high_water_mark{0}; // 0 = unlimited

    };

    struct Callbacks {
        CREATE_CALLBACK(OnConnected, void(tcp_session&))
        CREATE_CALLBACK(OnNegotiated, void(tcp_session&))
        CREATE_CALLBACK(OnDisconnected, void(tcp_session&, std::exception_ptr))
        CREATE_CALLBACK(OnDataAvailable, void(tcp_session&, const std::byte*, size_t))

        /** Callbacks:
         *
         *  - onNegotiated
         *
         *    Called once the session is fully up and carrying data: after a negotiated transport
         *    finishes its handshake, or immediately for a plain one (whose negotiation is a no-op).
         *    Fires after onConnected and before the first onDataAvailable, with the session in the
         *    `running` state so it accepts write(). Not called if the handshake fails or a stop
         *    races it -- onDisconnected carries the reason in that case. */
        OnConnected onConnected{};
        OnNegotiated onNegotiated{};
        OnDisconnected onDisconnected{};
        OnDataAvailable onDataAvailable{};
    };

    // For legacy
    typedef Callbacks::OnConnected on_connected_cb_t;
    typedef Callbacks::OnNegotiated on_negotiated_cb_t;
    typedef Callbacks::OnDisconnected on_disconnected_cb_t;
    typedef Callbacks::OnDataAvailable on_data_available_cb_t;

    enum class state_t {running, handshaking, stop_requested, stopping, stopped };

    /** @param factory builds the transport that carries the session's bytes, given the session's
     *         own socket; see sg/net/transport.h. Empty gives plain TCP; TLS is
     *         tls_transport_factory(). The transport is built here and lives for the session. */
    static std::shared_ptr<tcp_session> create(boost::asio::io_context& context,
                                               boost::asio::ip::tcp::socket socket,
                                               transport_factory factory,
                                               Callbacks callbacks,
                                               options_t options);

    tcp_session(private_tag, boost::asio::io_context& context, boost::asio::ip::tcp::socket socket,
                transport_factory factory, Callbacks cb, options_t options);
    ~tcp_session();

    void start();

    /** Requests a graceful stop and returns immediately.
     *
     *  Queued writes are drained first, so if a write is in flight this does not complete until
     *  that write finishes or hits @c options_t::timeout_msec. Use @c stop_async_force() if you
     *  would rather drop the pending data than wait. */
    void stop_async();

    /** As @c stop_async(), but closes the socket at once instead of draining.
     *
     *  Any queued or in-flight write is abandoned, so data already passed to @c write() may not
     *  reach the peer. The disconnection is still reported as clean. Safe to call after
     *  @c stop_async() to escalate a graceful stop that is taking too long. */
    void stop_async_force();

    void wait_until_stopped() const;

    /** Blocks until the session has settled either way: it is up and usable, or it has stopped.
     *
     *  Must not be called from a callback. */
    void wait_until_ready() const;

    /** returns true only while the session is running and can carry data, and false as soon as a
     * disconnection is requested or in progress.
     *
     * A session whose transport is still negotiating is NOT connected: it has been reported through
     * OnConnected (so callers can find it), but @c write() refuses data until the handshake lands
     * and it reaches @c running. Use @c state() if you need to tell the phases apart. */
    [[nodiscard]] bool is_connected() const noexcept;
    [[nodiscard]] state_t state() const noexcept;

    /** Whether the transport negotiates before it can carry data (TLS does, plain TCP does not).
     *  A negotiated session is not usable -- not @c is_connected() -- until its handshake lands. */
    [[nodiscard]] bool is_negotiated() const noexcept;

    /** @brief The error that stopped the session, or @c nullptr if it stopped cleanly.
     *
     * The same exception OnDisconnected is handed. Useful where there is no callback to read it
     * from -- a blocking caller that wants to know why a TLS handshake was refused, say. */
    [[nodiscard]] std::exception_ptr last_error() const;

    [[nodiscard]] end_point local_endpoint() const;
    [[nodiscard]] end_point remote_endpoint() const;

    void write(sg::shared_c_buffer<std::byte> msg);
    void write(std::string_view msg);
    void write(const void* data, size_t size);

    /** Bytes given to @c write() that the socket has not taken yet: those still queued, plus the
     *  batch currently being written.
     *
     *  Bytes a stopped session never managed to write stay counted, so this is non-zero after a
     *  @c stop_async_force() that dropped data. */
    [[nodiscard]] size_t pending_bytes() const noexcept;

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

    /* Carries the bytes over m_socket. Never null. Declared after it so it is destroyed first. */
    std::unique_ptr<tcp_transport> m_transport;

    /* Per-session strand. The socket is not thread-safe, so the reader, writer and close handlers
     * (which all touch it) must not run concurrently. Routing all three through this strand
     * serialises them. It does NOT serialise the underlying I/O. */
    boost::asio::strand<boost::asio::ip::tcp::socket::executor_type> m_strand;

    boost::asio::io_context::executor_type m_io_executor;

    options_t m_options;
    Callbacks m_callbacks;
    std::atomic<bool> m_disconnection_callback_owed{false};

    end_point m_local_endpoint {};
    end_point m_remote_endpoint {};

    std::mutex m_write_mutex;
    bool m_write_scheduled{false}; //note: need to always lock m_write_mutex
    std::vector<sg::shared_c_buffer<std::byte>> m_write_msgs{};

    /* Bytes in m_write_msgs (written under m_write_mutex) and bytes in the batch writer() is
     * sending (written by writer() alone). Atomic so that pending_bytes() can read both without
     * taking the lock. */
    std::atomic<size_t> m_queued_bytes{0};
    std::atomic<size_t> m_writing_bytes{0};

    std::atomic<state_t> m_state{state_t::stopped};
    std::atomic_flag m_start_called{};

    mutable std::mutex m_exception_mutex;
    std::exception_ptr m_exception;

    /* Records why the session is stopping, if it was still up. First writer wins: the original
     * cause must not be masked by the errors its own teardown provokes. Call from a catch block. */
    void record_error();

    /* Runs a member coroutine on the session strand, holding the session alive for its duration. */
    void spawn(boost::asio::awaitable<void> (tcp_session::*coro)());

    /* Blocks until state() satisfies `done`. `caller` names the public function, for the error
     * raised when this is called from the I/O pool. */
    void wait_until(const char* caller, bool (*done)(state_t)) const;

    /* `graceful` lets a negotiated transport sign off with the peer before the socket goes, which
     * costs a round trip. Only the drain path wants that; an error or a forced stop does not. */
    void close(bool graceful = false);
    void close_impl();

    /* Raw socket-option work. NOT thread-safe with respect to other socket access — callers must
     * either be running on m_strand or be in a phase where no other thread can touch the
     * socket (e.g. start(), before the reader/writer coroutines are spawned). */
    void apply_keepalive_unsafe(keepalive_t);
    void apply_timeout_unsafe(unsigned timeoutMSec);

    boost::asio::awaitable<void> reader();
    boost::asio::awaitable<void> writer();

    /* The whole life of a session whose transport negotiates: handshake, then read. A plaintext
     * session has nothing to negotiate, so start() spawns reader() directly instead. */
    boost::asio::awaitable<void> negotiate_and_read();
    /* Lets the transport sign off with the peer, then hands over to close_impl(). */
    boost::asio::awaitable<void> close_gracefully();
};

}
