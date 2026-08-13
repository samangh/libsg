#include <catch2/catch_test_macros.hpp>

#include <sg/net/asio_io_pool.h>
#include <sg/net/tcp_client.h>
#include <sg/net/tcp_server.h>

#include <atomic>
#include <ctime>
#include <memory>
#include <mutex>
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

TEST_CASE("tcp_session: a stopped io pool does not wedge the socket-option setters",
          "[sg::net::tcp_session]") {
    tcp_server server;
    server.start({ep}, {});

    /* a shared pool, as if it were owned by something else too */
    auto pool = asio_io_pool::create(1, false, nullptr);

    tcp_client client(pool);
    client.connect(ep, nullptr, nullptr);

    /* the other owner of the pool shuts it down behind the session's back */
    pool->stop_async();
    pool->wait_for_stop();

    auto& session = client.session();

    /* these used to block forever */
    REQUIRE_THROWS_AS(session.set_timeout(1000), std::runtime_error);
    REQUIRE_THROWS_AS(session.set_keepalive(keepalive_t{}), std::runtime_error);
}

TEST_CASE("tcp_session: a stopped io pool does not wedge teardown", "[sg::net::tcp_session]") {
    tcp_server server;
    server.start({ep}, {});

    std::atomic_int disconnections{0};
    tcp_session::Callbacks::OnDisconnected onDisconnected = [&](tcp_session&, std::exception_ptr) {
        ++disconnections;
    };

    auto pool = asio_io_pool::create(1, false, nullptr);

    tcp_client client(pool);
    client.connect(ep, nullptr, onDisconnected);

    /* queue a write, so that stop_async() would normally leave the close to the writer coroutine */
    client.session().write("hello");

    pool->stop_async();
    pool->wait_for_stop();

    auto& session = client.session();

    /* this used to hang, both with and without a pending write */
    session.stop_async();
    session.wait_until_stopped();

    REQUIRE(session.state() == tcp_session::state_t::stopped);
    REQUIRE(disconnections == 1);
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

/* io_context::stop() takes effect immediately, but a worker already inside a handler runs to
 * completion -- so a callback can find the context stopped while it is itself on the strand. The
 * work still runs there (dispatch() is inline), so run_in_executor() must not fail fast. */
TEST_CASE("tcp_session: set_timeout() from a callback still works once the pool is stopping",
          "[sg::net::tcp_session]") {
    tcp_server::CallBacks serverCbs;
    serverCbs.OnSessionCreated = [](tcp_server& s, tcp_server::session_id_t id) {
        s.write(id, "hello");
    };

    tcp_server server;
    server.start({ep}, serverCbs);

    auto pool = asio_io_pool::create(1, false, nullptr);

    std::atomic<bool> threw{false};
    std::atomic<bool> onStrand{false};
    std::atomic<bool> contextStopped{false};
    std::atomic<bool> fired{false};
    std::binary_semaphore done{false};

    tcp_session::Callbacks::OnDataAvailable onData = [&](tcp_session& s, const std::byte*, size_t) {
        if (fired.exchange(true))
            return;

        /* stop the pool from inside a handler: the context is stopped from here on, but this
         * handler is still running on the strand */
        pool->stop_async();

        onStrand = s.running_in_io_thread();
        contextStopped = !pool->is_running();

        try {
            s.set_timeout(1000);
        } catch (...) {
            threw = true;
        }
        done.release();
    };

    tcp_client client(pool);
    client.connect(ep, onData, nullptr);

    done.acquire();

    REQUIRE(onStrand);        // we are on the strand
    REQUIRE(contextStopped);  // and the context has stopped
    REQUIRE_FALSE(threw);     // yet the inline dispatch still works
}
