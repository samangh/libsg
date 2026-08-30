#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sg/jthread.h>
#include <sg/random.h>
#include <sg/net/tcp_client.h>
#include <sg/net/tcp_native.h>
#include <sg/net/tcp_server.h>

#include "helpers.h"

#include <chrono>
#include <thread>

using namespace sg::net;

// port 55555 can't be used on macOS!
static end_point ep("127.0.0.1", 4444);

TEST_CASE("tcp_client: check connect", "[sg::net::tcp_client]") {
    std::string msg = "hello";
    std::string expectedEcho = "hellohello";
    std::string result;
    std::binary_semaphore can_stop{0};

    /* tcp_server: echo back the message twice */
    tcp_server::session_data_available_cb_t on_data =
        [](tcp_server& srv, tcp_server::session_id_t sess, const std::byte* dat, size_t size) {
            auto echo = std::string((char*)dat, size) + std::string((char*)dat, size);
            srv.write(sess, echo.c_str(), echo.size());
        };

    sg::net::tcp_server server;
    tcp_server::CallBacks callbacks;
    callbacks.OnSessionDataAvailable = on_data;
    server.start({ep}, callbacks);

    /* client: collect echo response */
    tcp_session::Callbacks::OnDataAvailable onClientdata = [&](tcp_session&, const std::byte* dat, size_t size) {
        result = std::string((char*)dat, size);
        can_stop.release();
    };

    auto client  = tcp_client();
    client.connect(ep, onClientdata, nullptr);
    client.session().write(msg);

    // wait until we have readback
    can_stop.acquire();

    client.session().stop_async();
    client.session().wait_until_stopped();

    REQUIRE(result == expectedEcho);
}

TEST_CASE("tcp_client: check disconnect()", "[sg::net::tcp_client]") {
    std::binary_semaphore can_stop{0};

    tcp_server server;
    tcp_server::CallBacks callbacks;
    callbacks.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t,
                                   std::exception_ptr) { can_stop.release(); };

    server.start({ep}, callbacks);

    auto client  = tcp_client();
    client.connect(ep, nullptr, nullptr);
    client.session().write("hello");
    client.disconnect();

    // wait until we have readback
    can_stop.acquire();
}

TEST_CASE("tcp_client: check destructor of unused client", "[sg::net::tcp_client]") {
    tcp_client client;
}

TEST_CASE("tcp_client: check that you can't connect twice", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto client = tcp_client();
    client.connect(end_point("8.8.8.8", 53), nullptr, nullptr);
    REQUIRE_THROWS(client.connect(end_point("8.8.8.8", 53), nullptr, nullptr));
}

TEST_CASE("tcp_client: check multiple reconnections", "[sg::net::tcp_client]") {
    /* This normally finishes in a few tens of milliseconds. It has been seen to hang once in CI
     * (gcc/static/release), consuming the whole 20-minute job budget in silence; it could not be
     * reproduced locally in ~745 runs (multi-core, single-core-pinned, and under CPU load), so the
     * trigger is not yet pinned down. The watchdog bounds any recurrence: it aborts with a message
     * (and a core) in seconds instead of hanging the runner, turning a silent stall into a signal.
     *
     * See https://github.com/samangh/libsg/issues/42
     */
    scoped_deadline watchdog("DEADLOCK: tcp_client multiple-reconnections test stalled",
                             std::chrono::seconds(120));

    const int noClients              = 5;
    const int noConnectiosnPerClient = 100;

    std::atomic<int> connections{0};
    std::atomic<int> disconnections{0};

    tcp_server::CallBacks callbacks;
    callbacks.OnSessionCreated = [&](tcp_server&, tcp_server::session_id_t) { ++connections; };

    callbacks.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t,
                                   std::exception_ptr) { ++disconnections; };

    tcp_server server;
    server.start({ep}, callbacks);

    tcp_session::options_t options;

    for (auto k = 0; k < noClients; k++) {
        auto client = tcp_client();
        for (int i = 0; i < noConnectiosnPerClient; ++i) {
            client.connect(ep, nullptr, nullptr,options);
            client.disconnect();

#ifdef _WIN32
            // Windows doesn't like too many immediate connections/disconnections from the same
            // port pair
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
#endif
        }
     }

    const int expected = noClients * noConnectiosnPerClient;

    /* connect() returns once the TCP handshake is complete, which can be before the server has
     * accepted the connection -- it may still be sitting in the listen backlog. stop_async()
     * closes the acceptor and discards whatever has not been accepted, so stopping the instant
     * the last connect() returns can lose the tail of the run (this test used to fail roughly
     * once in thirty with a count one short).
     *
     * So wait for the server to report every connection before stopping. The wait is bounded, so
     * a genuine regression still fails the REQUIRE below rather than hanging. */
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (connections.load() < expected && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    server.stop_async();
    server.future_get_once();

    REQUIRE(connections == expected);

    /* no separate wait needed for this one: future_get_once() does not return until every session
     * has finished and its OnDisconnected callback has run. */
    REQUIRE(disconnections == expected);
}

TEST_CASE("tcp_client: check multiple disconnects are OK", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto client = tcp_client();
    client.connect(end_point("8.8.8.8", 53), nullptr, nullptr);

    client.disconnect();
    client.disconnect();
}

TEST_CASE("tcp_client: check disconnect() without connect() is OK", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto client = tcp_client();
    client.disconnect();
    client.disconnect();
}


TEST_CASE("tcp_client: set_keepalive(...)", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto client = tcp_client();
    client.connect(end_point("8.8.8.8", 53), nullptr, nullptr);

    unsigned outsideRange = (unsigned)std::numeric_limits<int>::max() + 1;

    // Check range errors

    REQUIRE_THROWS(client.session().set_keepalive(keepalive_t{
        .enable = true, .idle_seconds = outsideRange, .interval_seconds = 1, .count = 5}));
    REQUIRE_THROWS(client.session().set_keepalive(keepalive_t{
        .enable = true, .idle_seconds = 1, .interval_seconds = outsideRange, .count = 5}));
    REQUIRE_THROWS(client.session().set_keepalive(keepalive_t{
        .enable = true, .idle_seconds = 1, .interval_seconds = 1, .count = outsideRange}));
}

TEST_CASE("tcp_client: set_timeout(...)", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto client = tcp_client();
    client.connect(end_point("8.8.8.8", 53), nullptr, nullptr);

    client.session().set_timeout(100);
}

TEST_CASE("tcp_client: test that connect(...) will timeout", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto client = tcp_client();

    tcp_session::options_t options;
    options.connection_timeout_msec=500;
    REQUIRE_THROWS(client.connect(end_point("87.5.66.1", 53), nullptr, nullptr, options));
}

TEST_CASE("tcp_client: set recv_buffer_size via options", "[sg::net::tcp_client]") {
    using namespace sg::net;

    tcp_session::options_t options;
    options.recv_buffer_size = 16384;

    tcp_server server;
    server.start({ep}, tcp_server::CallBacks{});

    auto client = tcp_client();
    client.connect(ep, nullptr, nullptr, options);

    /* exact, not >=: the getter reports the application-visible size, so it matches what was
     * asked for unless the kernel clamped it -- and 16384 is inside every sane kernel's range */
    auto actual = sg::net::native::get_recv_buffer_size(client.session().native_handle());
    REQUIRE(actual == 16384);
}

TEST_CASE("tcp_client: set send_buffer_size via options", "[sg::net::tcp_client]") {
    using namespace sg::net;

    tcp_session::options_t options;
    options.send_buffer_size = 16384;

    tcp_server server;
    server.start({ep}, tcp_server::CallBacks{});

    auto client = tcp_client();
    client.connect(ep, nullptr, nullptr, options);

    auto actual = sg::net::native::get_send_buffer_size(client.session().native_handle());
    REQUIRE(actual == 16384);
}

TEST_CASE("tcp_client: client disconnects when destructed (with shared asio_io_pool)",
          "[sg::net::tcp_client]") {
    using namespace sg::net;

    std::binary_semaphore serverDisconnSem{0};
    std::binary_semaphore clientDisconnSem{0};

    // Crate io pool
    auto pool = asio_io_pool::create(1, true, nullptr);
    pool->run();

    // server
    tcp_server server;
    tcp_server::CallBacks cbs;
    cbs.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        serverDisconnSem.release();
    };
    server.start({ep}, cbs);

    // client
    {
        auto client = tcp_client(pool);
        tcp_session::Callbacks::OnDisconnected onDisconnected = [&](tcp_session&, std::exception_ptr) {
            clientDisconnSem.release();
        };
        client.connect(ep, nullptr, onDisconnected, {});
    }

    // wait for destructor
    serverDisconnSem.acquire();
    clientDisconnSem.acquire();
}