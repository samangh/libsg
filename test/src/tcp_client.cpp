#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sg/jthread.h>
#include <sg/random.h>
#include <sg/tcp_client.h>
#include <sg/tcp_server.h>

using namespace sg::net;

// port 55555 can't be used on macOS!
static end_point ep("127.0.0.1", 4444);

TEST_CASE("sg::net::tcp_client: check connect", "[sg::net::tcp_client]") {
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
    tcp_session::on_data_available_cb_t onClientdata = [&](tcp_session&, const std::byte* dat, size_t size) {
        result = std::string((char*)dat, size);
        can_stop.release();
    };

    auto context = asio_io_pool::create();
    auto client  = tcp_client(context);
    client.connect(ep, onClientdata, nullptr);
    client.session().write(msg);

    // wait until we have readback
    can_stop.acquire();

    client.session().stop_async();
    client.session().wait_until_stopped();

    REQUIRE(result == expectedEcho);
}

TEST_CASE("sg::net::tcp_client: check disconnect()", "[sg::net::tcp_client]") {
    std::binary_semaphore can_stop{0};

    tcp_server server;
    tcp_server::CallBacks callbacks;
    callbacks.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t,
                                   std::optional<std::exception>) { can_stop.release(); };

    server.start({ep}, callbacks);

    auto client  = tcp_client();
    client.connect(ep, nullptr, nullptr);
    client.session().write("hello");
    client.disconnect();

    // wait until we have readback
    can_stop.acquire();
}

TEST_CASE("sg::net::tcp_client: check that you can't connect twice", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto context = asio_io_pool::create();
    auto client = tcp_client(context);
    client.connect(end_point("8.8.8.8", 53), nullptr, nullptr);
    REQUIRE_THROWS(client.connect(end_point("8.8.8.8", 53), nullptr, nullptr));
}

TEST_CASE("sg::net::tcp_client: check multiple disconnects are OK", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto context = asio_io_pool::create();
    auto client = tcp_client(context);
    client.connect(end_point("8.8.8.8", 53), nullptr, nullptr);

    client.disconnect();
    client.disconnect();
}

TEST_CASE("sg::net::tcp_client: check disconnect() without connect() is OK", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto client = tcp_client();
    client.disconnect();
    client.disconnect();
}


TEST_CASE("sg::net::tcp_client: set_keepalive(...)", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto context = asio_io_pool::create();
    auto client = tcp_client(context);
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

TEST_CASE("sg::net::tcp_client: set_timeout(...)", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto context = asio_io_pool::create();
    auto client = tcp_client(context);
    client.connect(end_point("8.8.8.8", 53), nullptr, nullptr);

    client.session().set_timeout(100);
}

TEST_CASE("sg::net::tcp_client: check destruction of unused client", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto context = asio_io_pool::create();
    auto client = tcp_client(context);
}