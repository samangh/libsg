#include "helpers.h"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <sg/net/tcp_client_sync.h>
#include <sg/net/tcp_server.h>

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
        server.start({ep}, tcp_server::CallBacks());
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

    tcp_server::session_disconnected_cb_t Disconn= [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        disconn.release();
    };

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = onConn;
    cb.OnDisconnected=Disconn;
    server.start({ep}, cb);

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
                                                        std::exception_ptr) {
        disconn.release();
    };

    tcp_server::CallBacks cb;
    cb.OnDisconnected = Disconn;
    server.start({ep}, cb);

    {
        tcp_client_sync client;
        client.connect(ep);
    }
    disconn.acquire();
}

TEST_CASE("tcp_client_sync: check read_until()", "[sg::net::tcp_client_sync]") {
    std::binary_semaphore disconn{0};

    tcp_server server;
    tcp_server::session_created_cb_t onConn= [&](tcp_server&, tcp_server::session_id_t id) {
        server.write(id, "\nHELLO1\nHELLO2\nHELLO3\n");
    };

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = onConn;
    server.start({ep}, cb);

    tcp_client_sync client;
    client.connect(ep);
    REQUIRE(client.read_until("\n") == "\n");
    REQUIRE(client.read_until("\n") == "HELLO1\n");
    REQUIRE(client.read_until("\n") == "HELLO2\n");
    REQUIRE(client.read_until("\n") == "HELLO3\n");
}

TEST_CASE("tcp_client_sync: check read_some()", "[sg::net::tcp_client_sync]") {
    tcp_server server;
    tcp_server::session_created_cb_t onConn= [&](tcp_server&, tcp_server::session_id_t id) {
        server.write(id, "1234567890");
    };

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = onConn;
    server.start({ep}, cb);

    tcp_client_sync client;
    client.connect(ep);

    // throw error if this takes longer than 1 second
    scoped_deadline timeout("read_some failed", std::chrono::seconds(1));

    std::string result;
    while (true) {
        result += client.read_some();
        if (result == "1234567890")
            break;
    };
}

TEST_CASE("tcp_client_sync: check read_some() returns short reads", "[sg::net::tcp_client_sync]") {
    tcp_server server;
    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server& s, tcp_server::session_id_t id) { s.write(id, "123"); };
    server.start({ep}, cb);

    tcp_client_sync client;
    client.connect(ep);
    client.set_timeout(500);

    /* only 3 bytes are ever sent, read_some() must return those rather than wait for 10 */
    REQUIRE(client.read_some() == "123");
}

TEST_CASE("tcp_client_sync: check read()", "[sg::net::tcp_client_sync]") {
    tcp_server server;
    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server& s, tcp_server::session_id_t id) {
        s.write(id, "\n1234567890");
    };
    server.start({ep}, cb);

    tcp_client_sync client;
    client.connect(ep);
    REQUIRE(client.read_until("\n") == "\n");
    REQUIRE(client.read(1) == "1");
    REQUIRE(client.read(2) == "23");
    REQUIRE(client.read(3) == "456");
    REQUIRE(client.read(4) == "7890");
}

TEST_CASE("tcp_client_sync: check read() times out on a short read", "[sg::net::tcp_client_sync]") {
    tcp_server server;
    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server& s, tcp_server::session_id_t id) { s.write(id, "123"); };
    server.start({ep}, cb);

    tcp_client_sync client;
    client.connect(ep);
    client.set_timeout(500);

    /* only 3 of the 10 bytes are ever sent */
    REQUIRE_THROWS_AS(client.read(10), sg::exceptions::net::time_out);

    /* the bytes that did arrive must be buffered, and the connection must still be usable */
    REQUIRE(client.is_connected());
    REQUIRE(client.read(3) == "123");
}

TEST_CASE("tcp_client_sync: check write()", "[sg::net::tcp_client_sync]") {
    std::binary_semaphore dataReceived{0};

    tcp_server server;
    tcp_server::session_data_available_cb_t onData = [&](tcp_server&, tcp_server::session_id_t id,
                                                         const std::byte* data, size_t size) {
        server.write(id, data, size);
    };

    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = onData;
    server.start({ep}, cb);

    tcp_client_sync client;
    client.connect(ep);
    client.write("1234567890\n");
    REQUIRE(client.read_until("\n")=="1234567890\n");
}

TEST_CASE("tcp_client_sync: buffered data does not survive a reconnect",
          "[sg::net::tcp_client_sync]") {
    std::atomic_int connections{0};

    tcp_server server;
    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server& s, tcp_server::session_id_t id) {
        if (++connections == 1)
            s.write(id, "a\r\nSTALE\r\n");
        else
            s.write(id, "FRESH\r\n");
    };
    server.start({ep}, cb);

    tcp_client_sync client;

    /* leaves "STALE\r\n" buffered in the client */
    client.connect(ep);
    REQUIRE(client.read_until("\r\n") == "a\r\n");
    client.disconnect();

    client.connect(ep);
    REQUIRE(client.read_until("\r\n") == "FRESH\r\n");
}

TEST_CASE("tcp_client_sync: buffered data does not survive a reconnect, read_some()",
          "[sg::net::tcp_client_sync]") {
    std::atomic_int connections{0};

    tcp_server server;
    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server& s, tcp_server::session_id_t id) {
        if (++connections == 1)
            s.write(id, "a\r\nSTALE");
        else
            s.write(id, "FRESH");
    };
    server.start({ep}, cb);

    tcp_client_sync client;

    /* leaves "STALE" buffered in the client */
    client.connect(ep);
    REQUIRE(client.read_until("\r\n") == "a\r\n");
    client.disconnect();

    client.connect(ep);
    REQUIRE(client.read(5) == "FRESH");
}

TEST_CASE("tcp_client_sync: check reading when disconnected throws an error", "[sg::net::tcp_client_sync]") {
    tcp_client_sync client;
    REQUIRE_THROWS(client.read_until("\n"));
    REQUIRE_THROWS(client.read_some());
    REQUIRE_THROWS(client.read(10));
    REQUIRE_THROWS(client.write("ss"));
}

TEST_CASE("tcp_client_sync: check timeout()", "[sg::net::tcp_client_sync]") {
    std::binary_semaphore dataReceived{0};

    tcp_server server;
    server.start({ep}, {});

    tcp_client_sync client;
    client.connect(ep);
    client.set_timeout(100);
    REQUIRE_THROWS(client.read_until("\n"));
}