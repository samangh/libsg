#include <catch2/catch_test_macros.hpp>

#include <sg/tcp_client_sync.h>
#include <sg/tcp_server.h>

using namespace sg::net;

// port 55555 can't be used on macOS!
static end_point ep("127.0.0.1", 4444);

TEST_CASE("tcp_client_sync: unused client can be destructed", "[sg::net::tcp_client_sync]") {
    tcp_client_sync client;
}

TEST_CASE("tcp_client_sync: check destructor works after disconnect", "[sg::net::tcp_client_sync]") {
    tcp_client_sync client;
    {
        tcp_server server;
        server.start({ep}, nullptr, nullptr, nullptr, nullptr, nullptr);
        client.connect(ep);
    }
}

TEST_CASE("tcp_client_sync: check connect/disconn", "[sg::net::tcp_client_sync]") {
    std::binary_semaphore connected{0};
    std::binary_semaphore disconn{0};

    tcp_server server;
    tcp_server::session_created_cb_t onConn= [&](tcp_server&, tcp_server::session_id_t) {
        connected.release();
    };

    tcp_server::session_disconnected_cb_t Disconn= [&](tcp_server&, tcp_server::session_id_t, std::optional<std::exception>) {
        disconn.release();
    };

    server.start({ep}, nullptr, nullptr, onConn, nullptr, Disconn);

    tcp_client_sync client;

    // Check connection
    client.connect(ep);
    connected.acquire();

    //Check disconn
    client.disconnect();
    disconn.acquire();
}

TEST_CASE("tcp_client_sync: check disconnection on destructor", "[sg::net::tcp_client_sync]") {
    std::binary_semaphore disconn{0};

    tcp_server server;
    tcp_server::session_disconnected_cb_t Disconn = [&](tcp_server&, tcp_server::session_id_t,
                                                        std::optional<std::exception>) {
        disconn.release();
    };

    server.start({ep}, nullptr, nullptr, nullptr, nullptr, Disconn);

    {
        tcp_client_sync client;
        client.connect(ep);
    }
}

TEST_CASE("tcp_client_sync: check read_until()", "[sg::net::tcp_client_sync]") {
    std::binary_semaphore disconn{0};

    tcp_server server;
    tcp_server::session_created_cb_t onConn= [&](tcp_server&, tcp_server::session_id_t id) {
        server.write(id, "\nHELLO1\nHELLO2\nHELLO3\n");
    };
    server.start({ep}, nullptr, nullptr, onConn, nullptr, nullptr);

    tcp_client_sync client;
    client.connect(ep);
    REQUIRE(client.read_until("\n") == "\n");
    REQUIRE(client.read_until("\n") == "HELLO1\n");
    REQUIRE(client.read_until("\n") == "HELLO2\n");
    REQUIRE(client.read_until("\n") == "HELLO3\n");
}

TEST_CASE("tcp_client_sync: check read_some()", "[sg::net::tcp_client_sync]") {
    std::binary_semaphore disconn{0};

    tcp_server server;
    tcp_server::session_created_cb_t onConn= [&](tcp_server&, tcp_server::session_id_t id) {
        server.write(id, "\n1234567890");
    };
    server.start({ep}, nullptr, nullptr, onConn, nullptr, nullptr);

    tcp_client_sync client;
    client.connect(ep);
    REQUIRE(client.read_until("\n") == "\n");
    REQUIRE(client.read_some(1) == "1");
    REQUIRE(client.read_some(2) == "23");
    REQUIRE(client.read_some(3) == "456");
    REQUIRE(client.read_some(4) == "7890");
}

TEST_CASE("tcp_client_sync: check write()", "[sg::net::tcp_client_sync]") {
    std::binary_semaphore dataReceived{0};

    tcp_server server;
    tcp_server::session_data_available_cb_t onData = [&](tcp_server&, tcp_server::session_id_t id,
                                                         const std::byte* data, size_t size) {
        server.write(id, data, size);
    };
    server.start({ep}, nullptr, nullptr, nullptr, onData, nullptr);

    tcp_client_sync client;
    client.connect(ep);
    client.write("1234567890\n");
    REQUIRE(client.read_until("\n")=="1234567890\n");
}

TEST_CASE("tcp_client_sync: check reading when disconnected throws an error", "[sg::net::tcp_client_sync]") {
    tcp_client_sync client;
    REQUIRE_THROWS(client.read_until("\n"));
    REQUIRE_THROWS(client.read_some(10));
    REQUIRE_THROWS(client.write("ss"));
}
