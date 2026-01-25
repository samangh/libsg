#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sg/jthread.h>
#include <sg/random.h>
#include <sg/tcp_client.h>
#include <sg/tcp_server.h>

// port 55555 can't be used on macOS!
static sg::net::end_point ep("127.0.0.1", 4444);

TEST_CASE("sg::net::tcp_client: check connect", "[sg::net::tcp_client]") {
    using namespace sg::net;

        std::string msg = "hello";
        std::string result;
        std::binary_semaphore can_stop{0};

        tcp_server::session_data_available_cb_t on_data =
            [&](tcp_server&, tcp_server::session_id_t, const std::byte* dat, size_t size) {
                result = std::string((char*)dat, size);
                can_stop.release();
            };

        auto context = sg::net::tcp_context::create();

        sg::net::tcp_server server;
        server.start({ep}, nullptr, nullptr, nullptr, on_data, nullptr);

        auto client = sg::net::tcp_client(context);
        client.connect(ep, nullptr, nullptr);
        client.session().write(msg);

        can_stop.acquire();

        REQUIRE(result == msg);
}

TEST_CASE("sg::net::tcp_client: set_keepalive(...)", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto context = tcp_context::create();
    auto client = tcp_client(context);
    client.connect(end_point("8.8.8.8", 53), nullptr, nullptr);

    unsigned outsideRange = (unsigned)std::numeric_limits<int>::max() + 1;

    client.session().set_keepalive(true);

    // Check range errors
    REQUIRE_THROWS(client.session().set_keepalive(true, outsideRange));
    REQUIRE_THROWS(client.session().set_keepalive(true,1, outsideRange));
    REQUIRE_THROWS(client.session().set_keepalive(true,1, 1, outsideRange));
}

TEST_CASE("sg::net::tcp_client: set_timeout(...)", "[sg::net::tcp_client]") {
    using namespace sg::net;

    auto context = tcp_context::create();
    auto client = tcp_client(context);
    client.connect(end_point("8.8.8.8", 53), nullptr, nullptr);

    client.session().set_timeout(100);
}
