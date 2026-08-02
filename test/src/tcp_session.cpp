#include <catch2/catch_test_macros.hpp>

#include <sg/net/tcp_client.h>
#include <sg/net/tcp_server.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <tuple>

using namespace sg::net;

// port 55555 can't be used on macOS!
static end_point ep("127.0.0.1", 4444);

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
