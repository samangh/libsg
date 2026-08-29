#include <catch2/catch_test_macros.hpp>

#include "helpers.h"

#include <sg/net/tcp_client.h>
#include <sg/net/tcp_server.h>

#include <boost/asio.hpp>

#include <atomic>
#include <chrono>
#include <ctime>
#include <memory>
#include <mutex>
#include <semaphore>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
    #include <winsock2.h>
#else
    #include <sys/socket.h>
#endif

using namespace sg::net;

// port 55555 can't be used on macOS!
static end_point ep("127.0.0.1", 4444);

/* reads whatever is currently available on the session's socket */
static std::string read_native(tcp_session& session) {
    char buff[128];
    auto count = ::recv(session.native_handle(), buff, static_cast<int>(sizeof(buff)), 0);
    if (count <= 0)
        return {};
    return std::string(buff, static_cast<size_t>(count));
}

TEST_CASE("tcp_session: throwing onDisconnected, close driven by the user",
          "[sg::net::tcp_session]") {
    tcp_server server;
    server.start({ep}, {});

    std::atomic_int disconnections{0};
    tcp_session::Callbacks::OnDisconnected onDisconnected = [&](tcp_session&, std::exception_ptr) {
        ++disconnections;
        throw std::runtime_error("boom (this is expected, ignore)");
    };

    tcp_client client;
    client.connect(ep, nullptr, onDisconnected);
    client.disconnect();

    REQUIRE(disconnections == 1);
    REQUIRE(client.session().state() == tcp_session::state_t::stopped);
    REQUIRE_FALSE(client.is_connected());
}

TEST_CASE("tcp_session: throwing onDisconnected, close driven by a callback on server",
          "[sg::net::tcp_session]") {
    tcp_server::CallBacks serverCbs;
    serverCbs.OnSessionDataAvailable = [](tcp_server& s, tcp_server::session_id_t id,
                                          const std::byte*, size_t) { s.disconnect(id); };

    tcp_server server;
    server.start({ep}, serverCbs);

    std::atomic_int disconnections{0};
    tcp_session::Callbacks::OnDisconnected onDisconnected = [&](tcp_session&, std::exception_ptr) {
        ++disconnections;
        throw std::runtime_error("boom (this is expected, ignore)");
    };

    tcp_client client;
    client.connect(ep, nullptr, onDisconnected);

    auto& session = client.session();
    session.write("hello");

    session.wait_until_stopped();
    REQUIRE(disconnections == 1);
    REQUIRE_FALSE(session.is_connected());
}

TEST_CASE("tcp_session: dont_read, the callback reads the native handle","[sg::net::tcp_session]") {
    std::mutex mutex;
    std::string received;

    std::atomic<bool> callbackParametersBad {false};
    std::binary_semaphore dataWasReadByUs {false};

    /* server */
    tcp_server::CallBacks serverCbs;
    serverCbs.OnSessionCreated = [](tcp_server& s, tcp_server::session_id_t id) {
        s.write(id, "hello");
    };

    tcp_server server;
    server.start({ep}, serverCbs);

    /* setup client */
    tcp_session::Callbacks::OnDataAvailable onData = [&](tcp_session& s, const std::byte* data,
                                                        size_t size) {
        /* check the callback parameters are ok */
        if (data != nullptr || size != 0)
            callbackParametersBad = true;

        std::lock_guard lock(mutex);
        received += read_native(s);

        if (received == "hello")
            dataWasReadByUs.release();
    };

    tcp_session::options_t options;
    options.dont_read = true;

    tcp_client client;
    client.connect(ep, onData, nullptr, options);

    /* check */
    dataWasReadByUs.acquire();
    REQUIRE(callbackParametersBad == false);

    client.disconnect();
}

TEST_CASE("tcp_session: dont_read detects the peer disconnecting", "[sg::net::tcp_session]") {
    std::atomic<tcp_server::session_id_t> sessionId{0};
    std::binary_semaphore sessionCreated{false};

    std::atomic<int> disconnections{0};
    std::atomic<bool> exceptionReported{false};
    std::atomic<bool> dataCbCalled {false};

    /* server */
    tcp_server::CallBacks serverCbs;
    serverCbs.OnSessionCreated = [&](tcp_server&, tcp_server::session_id_t id) {
        sessionId = id;
        sessionCreated.release();
    };

    tcp_server server;
    server.start({ep}, serverCbs);

    /* client */
    tcp_session::Callbacks::OnDisconnected onDisconnected = [&](tcp_session&,
                                                               std::exception_ptr ex) {
        exceptionReported = (ex != nullptr);
        ++disconnections;
    };

    tcp_session::Callbacks::OnDataAvailable onData = [&](tcp_session&, const std::byte*, size_t) {
        dataCbCalled = true;
    };

    tcp_session::options_t options;
    options.dont_read = true;

    tcp_client client;
    client.connect(ep, onData, onDisconnected, options);

    // Wait for connection
    sessionCreated.acquire();

    // Disconnect
    server.stop_async();
    server.future_get_once();

    client.session().wait_until_stopped();

    REQUIRE(disconnections == 1);
    REQUIRE(dataCbCalled == false);
    REQUIRE(exceptionReported);
}


TEST_CASE("tcp_session: dont_read requires an OnDataAvailable callback", "[sg::net::tcp_session]") {
    tcp_server server;
    server.start({ep}, {});

    tcp_session::options_t options;
    options.dont_read = true;

    tcp_client client;
    REQUIRE_THROWS_AS(client.connect(ep, nullptr, nullptr, options), std::invalid_argument);
    REQUIRE_FALSE(client.is_connected());
}

TEST_CASE("tcp_session: throwing onDisconnected, close driven by callback on client",
          "[sg::net::tcp_session]") {
    tcp_server::CallBacks serverCbs;
    serverCbs.OnSessionDataAvailable = [](tcp_server& s, tcp_server::session_id_t id,
                                          const std::byte* data, size_t size) {
        s.session(id)->write(data, size);
    };

    tcp_server server;
    server.start({ep}, serverCbs);

    tcp_session::Callbacks::OnDataAvailable onData = [](tcp_session& s, const std::byte*, size_t) {
        s.stop_async();
    };

    std::atomic_int disconnections{0};
    tcp_session::Callbacks::OnDisconnected onDisconnected = [&](tcp_session&, std::exception_ptr) {
        ++disconnections;
        throw std::runtime_error("boom (this is expected, ignore)");
    };

    tcp_client client;
    client.connect(ep, onData, onDisconnected);

    auto& session = client.session();
    session.write("hello");

    session.wait_until_stopped();
    REQUIRE(disconnections == 1);
    REQUIRE_FALSE(session.is_connected());
}

TEST_CASE("tcp_session: running_in_io_thread() sees sibling sessions on the same pool",
          "[sg::net::tcp_session]") {
    scoped_deadline watchdog("DEADLOCK: blocking call on a sibling session sharing one pool",
                             std::chrono::seconds(10));

    /* echo server, so that client a's OnDataAvailable fires on demand */
    tcp_server::CallBacks serverCbs;
    serverCbs.OnSessionDataAvailable = [](tcp_server& s, tcp_server::session_id_t id,
                                          const std::byte* data, size_t size) {
        s.session(id)->write(data, size);
    };

    tcp_server server;
    server.start({ep}, serverCbs);

    /* one worker, shared by both clients: a's callback occupies the pool's only thread */
    auto pool = asio_io_pool::create(1, true, nullptr);
    pool->run();

    tcp_client a(pool);
    tcp_client b(pool);

    std::atomic<bool> ownStrand{false};
    std::atomic<bool> siblingSession{false};
    std::atomic<bool> disconnectRefused{false};
    std::binary_semaphore done{0};

    b.connect(ep, nullptr, nullptr);

    a.connect(
        ep,
        [&](tcp_session& sess, const std::byte*, size_t) {
            ownStrand      = sess.running_in_io_thread();
            siblingSession = b.session().running_in_io_thread();

            /* b's teardown has to run on the very worker we are occupying, so waiting for it here
             * must be refused rather than attempted */
            try {
                b.disconnect();
            } catch (const std::logic_error&) {
                disconnectRefused = true;
            }

            done.release();
        },
        nullptr);

    a.session().write("ping");
    done.acquire();

    REQUIRE(ownStrand);       // our own strand: true before and after the widening
    REQUIRE(siblingSession);  // the sibling's strand: only true once the guard is pool-wide
    REQUIRE(disconnectRefused);
}

TEST_CASE("tcp_session: stop_async_force() does not wait for an in-flight write",
          "[sg::net::tcp_session]") {
    constexpr unsigned timeout_msec = 2000;
    scoped_deadline watchdog("stop_async_force() blocked on an in-flight write",
                             std::chrono::seconds(20));

    /* A deaf peer: accepts the connection and never reads from it. The small receive buffer is set
     * on the acceptor so the accepted socket inherits it, which keeps the amount we have to write
     * before the peer's window closes small. */
    end_point peer_ep("127.0.0.1", 4455);
    boost::asio::io_context peer_ctx;
    boost::asio::ip::tcp::acceptor acceptor(
        peer_ctx, {boost::asio::ip::make_address(peer_ep.ip), peer_ep.port});
    acceptor.set_option(boost::asio::socket_base::receive_buffer_size(2048));
    boost::asio::ip::tcp::socket peer(peer_ctx);

    std::exception_ptr disconnect_ex;
    tcp_session::Callbacks::OnDisconnected onDisc = [&](tcp_session&, std::exception_ptr ex) {
        disconnect_ex = ex;
    };

    tcp_client client;
    {
        std::jthread accepting([&] { acceptor.accept(peer); });
        client.connect(peer_ep, nullptr, onDisc,
                       {.timeout_msec = timeout_msec, .send_buffer_size = 4096});
    }

    /* Far more than the send buffer plus the peer's receive window, so async_write cannot
     * complete and writer() is left parked on it. */
    client.session().write(std::string(8u << 20, 'x'));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto start = std::chrono::steady_clock::now();
    client.session().stop_async_force();
    client.session().wait_until_stopped();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    /* stop_async() would have waited out timeout_msec here; the generous bound keeps this from
     * being a timing-sensitive test while still failing loudly if the write is drained. */
    CAPTURE(elapsed.count());
    REQUIRE(elapsed < std::chrono::milliseconds(timeout_msec / 4));

    // abandoning the write is not an error: the disconnection is still reported as clean
    REQUIRE(disconnect_ex == nullptr);

    peer.close();
}

TEST_CASE("tcp_session: start() can only be called once", "[sg::net::tcp_session]") {
    tcp_server server;
    server.start({ep}, {});

    std::atomic_int disconnections{0};
    tcp_session::Callbacks::OnDisconnected onDisconnected = [&](tcp_session&, std::exception_ptr) {
        ++disconnections;
    };

    tcp_client client;
    client.connect(ep, nullptr, onDisconnected);

    /* while the session is still running */
    REQUIRE_THROWS_AS(client.session().start(), std::logic_error);
    REQUIRE(client.session().state() == tcp_session::state_t::running);
    REQUIRE(client.is_connected());
    REQUIRE(disconnections == 0);

    client.disconnect();
    REQUIRE(disconnections == 1);

    /* and once it has stopped -- the case the state machine cannot catch on its own. The guard is
     * checked before start() looks at any state, so it holds however the session came to stop. */
    REQUIRE_THROWS_AS(client.session().start(), std::logic_error);
    REQUIRE(client.session().state() == tcp_session::state_t::stopped);
    REQUIRE(disconnections == 1);
}


TEST_CASE("tcp_session: is_connected() is false once a stop has been requested",
          "[sg::net::tcp_session]") {
    scoped_deadline watchdog("is_connected()/stop_requested test stalled");

    boost::asio::io_context ctx;
    boost::asio::ip::tcp::acceptor acceptor(
        ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(ep.ip), ep.port));

    /* the accepted socket inherits this, so the peer's receive window stays tiny and a couple of
     * MB is already more than both ends can hold */
    acceptor.set_option(boost::asio::socket_base::receive_buffer_size(4096));

    boost::asio::ip::tcp::socket peer(ctx);
    std::thread acc([&] { acceptor.accept(peer); });

    tcp_client client;
    client.connect(ep, nullptr, nullptr, {.timeout_msec = 30000, .send_buffer_size = 4096});
    acc.join();

    REQUIRE(client.session().state() == tcp_session::state_t::running);
    REQUIRE(client.session().is_connected());

    /* the peer never reads, so this write stays in flight and the graceful stop cannot complete:
     * the session is parked in stop_requested */
    for (int i = 0; i < 8; ++i)
        client.session().write(std::string(256u << 10, 'x'));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    client.session().stop_async();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    REQUIRE(client.session().state() == tcp_session::state_t::stop_requested);
    REQUIRE_FALSE(client.session().is_connected());

    // and the predicate agrees with what write() actually does in that state
    REQUIRE_THROWS(client.session().write("more"));

    client.session().stop_async_force();
    client.session().wait_until_stopped();
    peer.close();
}

TEST_CASE("tcp_client: disconnect() waits out a teardown already in progress",
          "[sg::net::tcp_client]") {
    scoped_deadline watchdog("disconnect() did not wait for the teardown to finish");

    boost::asio::io_context ctx;
    boost::asio::ip::tcp::acceptor acceptor(
        ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(ep.ip), ep.port));

    /* the accepted socket inherits this, so the peer's receive window stays tiny and a couple of
     * MB is already more than both ends can hold. Queueing that little is near-instant, which
     * matters: the writer's timeout starts on the *first* write, so queueing bulk data (say
     * 8 x 64MB, which a Debug/sanitizer build takes over a second to allocate and copy) races the
     * timeout and the session is torn down while we are still writing to it */
    acceptor.set_option(boost::asio::socket_base::receive_buffer_size(4096));

    boost::asio::ip::tcp::socket peer(ctx);
    std::thread acc([&] { acceptor.accept(peer); });

    tcp_client client;
    client.connect(ep, nullptr, nullptr, {.timeout_msec = 1000, .send_buffer_size = 4096});
    acc.join();

    /* the peer never reads, so this write stays in flight and the graceful stop cannot complete */
    for (int i = 0; i < 8; ++i)
        client.session().write(std::string(256u << 10, 'x'));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client.session().stop_async();
    REQUIRE(client.session().state() == tcp_session::state_t::stop_requested);
    REQUIRE_FALSE(client.session().is_connected()); // already past running

    /* must still block until the session is genuinely stopped, which the in-flight write only
     * lets happen once it hits timeout_msec */
    client.disconnect();
    REQUIRE(client.session().state() == tcp_session::state_t::stopped);

    peer.close();
}

TEST_CASE("tcp_session: pending_bytes() drains to zero", "[sg::net::tcp_session]") {
    scoped_deadline watchdog("pending_bytes() never drained");

    tcp_server server;
    server.start({ep}, {});

    tcp_client client;
    client.connect(ep, nullptr, nullptr);

    auto& session = client.session();
    REQUIRE(session.pending_bytes() == 0);

    session.write(std::string(1u << 20, 'x'));
    while (session.pending_bytes() != 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    client.disconnect();
}

TEST_CASE("tcp_session: pending_bytes() counts what the peer has not taken",
          "[sg::net::tcp_session]") {
    scoped_deadline watchdog("pending_bytes() test did not finish");

    constexpr size_t size = 8u << 20;

    // a deaf peer, so nothing we write can be handed over in full
    end_point peer_ep("127.0.0.1", 4456);
    boost::asio::io_context peer_ctx;
    boost::asio::ip::tcp::acceptor acceptor(
        peer_ctx, {boost::asio::ip::make_address(peer_ep.ip), peer_ep.port});
    acceptor.set_option(boost::asio::socket_base::receive_buffer_size(2048));
    boost::asio::ip::tcp::socket peer(peer_ctx);

    tcp_client client;
    {
        std::jthread accepting([&] { acceptor.accept(peer); });
        client.connect(peer_ep, nullptr, nullptr, {.timeout_msec = 0, .send_buffer_size = 4096});
    }

    auto& session = client.session();
    REQUIRE(session.pending_bytes() == 0);

    session.write(std::string(size, 'x'));
    REQUIRE(session.pending_bytes() == size);

    // the data is dropped rather than sent, so it stays counted
    session.stop_async_force();
    session.wait_until_stopped();
    REQUIRE(session.pending_bytes() == size);

    peer.close();
}

TEST_CASE("tcp_session: write() refuses data at the high-water mark", "[sg::net::tcp_session]") {
    scoped_deadline watchdog("write() never reached the high-water mark");

    constexpr size_t chunk = 64u << 10;
    constexpr size_t mark = 512u << 10;

    end_point peer_ep("127.0.0.1", 4457);
    boost::asio::io_context peer_ctx;
    boost::asio::ip::tcp::acceptor acceptor(
        peer_ctx, {boost::asio::ip::make_address(peer_ep.ip), peer_ep.port});
    acceptor.set_option(boost::asio::socket_base::receive_buffer_size(2048));
    boost::asio::ip::tcp::socket peer(peer_ctx);

    tcp_client client;
    {
        std::jthread accepting([&] { acceptor.accept(peer); });
        client.connect(peer_ep, nullptr, nullptr,
                       {.timeout_msec = 0,
                        .send_buffer_size = 4096,
                        .write_high_water_mark = mark});
    }

    auto& session = client.session();

    bool refused = false;
    for (int i = 0; i < 64 && !refused; ++i)
        try {
            session.write(std::string(chunk, 'x'));
        } catch (const sg::exceptions::net::buffer_full&) {
            refused = true;
        }

    REQUIRE(refused);

    // a refused write is not fatal to the session
    REQUIRE(session.is_connected());

    session.stop_async_force();
    session.wait_until_stopped();
    peer.close();
}
