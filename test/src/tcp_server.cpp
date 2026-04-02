#include <catch2/catch_test_macros.hpp>

#include "sg/tcp_client.h"
#include "sg/tcp_client_sync.h"
#include "sg/jthread.h"
#include "sg/random.h"
#include "sg/tcp_server.h"

#include <boost/asio.hpp>

#include <atomic>
#include <semaphore>
#include <string>

using namespace sg::net;
static port_t PORT = 4444; // 55555 can't be used on macOS!

TEST_CASE("tcp_server: check bad endpoint throws exception during start()", "[sg::net::tcp_server]") {
    end_point ep;
    ep.ip = PORT;
    ep.ip = "8.8.8.8";

    tcp_server l;
    REQUIRE_THROWS(l.start({ep}, decltype(l)::CallBacks()));
}

TEST_CASE("tcp_server: check start/stop callback", "[sg::net::tcp_server]") {
    std::atomic_int stop_count{0};
    std::binary_semaphore start_sem{0};

    using namespace sg::net;

    tcp_server::started_listening_cb_t onStart = [&](tcp_server&) { start_sem.release(); };
    tcp_server::stopped_listening_cb_t onStop  = [&](tcp_server&) { stop_count++; };

    tcp_server::session_disconnected_cb_t onDisconn =
        [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) { stop_count++; };

    end_point ep("127.0.0.1", PORT);

    tcp_server::CallBacks cb;
    cb.OnStartedListening = onStart;
    cb.OnStoppedListening = onStop;

    tcp_server l;
    l.start({ep}, cb);

    start_sem.acquire();
    REQUIRE(stop_count == 0);

    l.stop_async();
    l.future_get_once();
    REQUIRE(stop_count == 1);
}

struct tcp_server_test0 {
    std::atomic_int stop_count{0};
    std::binary_semaphore start_sem{0};

    tcp_server l;
    void start() {
        end_point ep("127.0.0.1", PORT);
        auto onstart = std::bind(&tcp_server_test0::on_start, this, std::placeholders::_1);
        auto onstop  = std::bind(&tcp_server_test0::on_stop, this, std::placeholders::_1);

        tcp_server::CallBacks cb;
        cb.OnStartedListening = onstart;
        cb.OnStoppedListening = onstop;
        l.start({ep}, cb);
    }
    void on_start(tcp_server&) { start_sem.release(); }
    void on_stop(tcp_server&) { stop_count++; }
    void on_disconn(tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        stop_count++;
    }
};

TEST_CASE("tcp_server: check start/stop callback as class member", "[sg::net::tcp_server]") {
    tcp_server_test0 t;
    t.start();
    t.start_sem.acquire();
    REQUIRE(t.stop_count == 0);

    t.l.stop_async();
    t.l.future_get_once();
    REQUIRE(t.stop_count == 1);
}

TEST_CASE("tcp_server: check read/write with many simultaneous clients", "[sg::net::tcp_server]") {
    using namespace sg::net;

    int count = 100;
    std::atomic_int counterNew{0};
    std::atomic_int counterClosed{0};

    tcp_server::session_data_available_cb_t on_data = [](tcp_server& l, tcp_server::session_id_t id,
                                                         const std::byte* dat, size_t size) {
        auto w = sg::make_shared_c_buffer<std::byte>(size);
        std::memcpy(w.get(), dat, size);
        l.write(id, w);
    };

    tcp_server::session_created_cb_t onNew = [&counterNew](tcp_server&, tcp_server::session_id_t) {
        counterNew++;
    };

    tcp_server::session_disconnected_cb_t onClose = [&counterClosed,
                                                     count](tcp_server& l, tcp_server::session_id_t,
                                                            std::exception_ptr) {
        counterClosed++;
        if (counterClosed == count)
            l.stop_async();
    };

    end_point ep("0.0.0.0", PORT);

    tcp_server::CallBacks cb;
    cb.OnSessionCreated       = onNew;
    cb.OnSessionDataAvailable = on_data;
    cb.OnDisconnected         = onClose;

    tcp_server l;
    l.start({ep}, cb);

    auto func = []() {
        using boost::asio::ip::tcp;

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", std::to_string(PORT));

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        boost::system::error_code error;
        auto buf_write = sg::random::generate<char>(20);
        std::vector<char> buf_read(20);

        socket.write_some(boost::asio::buffer(buf_write), error);
        socket.read_some(boost::asio::buffer(buf_read), error);

        if (buf_write != buf_read)
            throw std::runtime_error("error");

        if (error)
            throw boost::system::system_error(error);

        socket.shutdown(tcp::socket::shutdown_type::shutdown_both);
        socket.close();
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < count; i++) threads.emplace_back(std::thread(func));

    for (auto& th : threads) REQUIRE_NOTHROW(th.join());

    l.future_get_once();

    REQUIRE(counterNew.load() == count);
    REQUIRE(counterClosed.load() == count);
}

TEST_CASE("tcp_server: check can disconnect client", "[sg::net::tcp_server]") {
    using namespace sg::net;

    tcp_server::session_data_available_cb_t on_data = [](tcp_server& l, tcp_server::session_id_t id,
                                                         const std::byte*,
                                                         size_t) { l.disconnect(id); };

    tcp_server::session_disconnected_cb_t on_disconn = [](tcp_server& l, tcp_server::session_id_t,
                                                          std::exception_ptr) {
        l.stop_async();
    };

    end_point ep("0.0.0.0", PORT);

    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = on_data;
    cb.OnDisconnected         = on_disconn;

    tcp_server l;
    l.start({ep}, cb);

    std::jthread th = std::jthread([]() {
        using boost::asio::ip::tcp;

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", std::to_string(PORT));

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        boost::system::error_code error;
        std::array<char, 5> buf_write = {'H', 'E', 'L', 'L', 'O'};
        socket.write_some(boost::asio::buffer(buf_write), error);

        for (;;) {
            std::array<char, 5> buf_read;

            socket.read_some(boost::asio::buffer(buf_read), error);
            if (error == boost::asio::error::eof)
                break;
            else if (error)
                throw boost::system::system_error(error);
        }
    });

    REQUIRE_NOTHROW(th.join());
}

TEST_CASE("tcp_server: check what happens if client disconnects", "[sg::net::tcp_server]") {
    using namespace sg::net;

    std::atomic_bool has_exception{false};

    tcp_server::session_disconnected_cb_t on_disconn = [&](tcp_server& l, tcp_server::session_id_t,
                                                           std::exception_ptr ex) {
        if (ex)
            has_exception = true;
        l.stop_async();
    };

    end_point ep("0.0.0.0", PORT);

    tcp_server::CallBacks cb;
    cb.OnDisconnected = on_disconn;

    tcp_server l;
    l.start({ep}, cb);

    std::jthread th = std::jthread([]() {
        using boost::asio::ip::tcp;

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", std::to_string(PORT));

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        boost::system::error_code error;
        std::array<char, 5> buf_write = {'H', 'E', 'L', 'L', 'O'};
        socket.write_some(boost::asio::buffer(buf_write), error);
    });

    REQUIRE_NOTHROW(th.join());

    l.future_get_once();
    REQUIRE(has_exception);
}

TEST_CASE("tcp_server started_listening_cb_t exception handling", "[sg::net::tcp_server]") {
    using namespace sg::net;

    tcp_server::started_listening_cb_t onListening = [](tcp_server&) {
        throw std::runtime_error("bad error!");
    };

    end_point ep("0.0.0.0", PORT);
    tcp_server l;

    tcp_server::CallBacks cb;
    cb.OnStartedListening = onListening;

    REQUIRE_THROWS(l.start({ep}, cb));
}

TEST_CASE("tcp_server stopped_listening_cb_t cb exception handling", "[sg::net::tcp_server]") {
    using namespace sg::net;

    tcp_server::stopped_listening_cb_t onStop = [](tcp_server&) {
        throw std::runtime_error("bad error!");
    };

    tcp_server::CallBacks cb;
    cb.OnStoppedListening = onStop;

    end_point ep("0.0.0.0", PORT);
    auto l = tcp_server();
    l.start({ep}, cb);
    l.stop_async();
    REQUIRE_THROWS(l.future_get_once());
}

TEST_CASE("tcp_server: check session(...)", "[sg::net::tcp_server]") {
    using namespace sg::net;

    tcp_server::session_data_available_cb_t on_data = [](tcp_server& l, tcp_server::session_id_t id,
                                                         const std::byte* dat, size_t size) {
        auto w = sg::make_shared_c_buffer<std::byte>(size);
        std::memcpy(w.get(), dat, size);
        l.session(id)->write(w);
    };

    tcp_server::session_disconnected_cb_t on_disconn = [](tcp_server& l, tcp_server::session_id_t,
                                                          std::exception_ptr) {
        l.stop_async();
    };

    end_point ep("0.0.0.0", PORT);
    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = on_data;
    cb.OnDisconnected         = on_disconn;

    tcp_server l;
    l.start({ep}, cb);

    std::jthread th = std::jthread([]() {
        using boost::asio::ip::tcp;

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", std::to_string(PORT));

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        boost::system::error_code error;
        std::array<char, 5> buf_write = {'H', 'E', 'L', 'L', 'O'};
        socket.write_some(boost::asio::buffer(buf_write), error);

        for (;;) {
            std::array<char, 5> buf_read;

            socket.read_some(boost::asio::buffer(buf_read), error);
            if (buf_read == buf_write)
                break;
            if (error)
                throw boost::system::system_error(error);
        }
    });

    REQUIRE_NOTHROW(th.join());
    l.future_get_once();
}

TEST_CASE("tcp_server: check local/remote_endpoint(...)", "[sg::net::tcp_server]") {
    using namespace sg::net;

    tcp_server::session_data_available_cb_t on_data = [](tcp_server& l, tcp_server::session_id_t id,
                                                         const std::byte* dat, size_t size) {
        auto w = sg::make_shared_c_buffer<std::byte>(size);
        std::memcpy(w.get(), dat, size);

        auto local  = l.session(id)->local_endpoint();
        auto remote = l.session(id)->remote_endpoint();
        REQUIRE(local.ip == remote.ip);
        REQUIRE(local.port == PORT);

        l.session(id)->write(w);
    };

    tcp_server::session_disconnected_cb_t on_disconn = [](tcp_server& l, tcp_server::session_id_t,
                                                          std::exception_ptr) {
        l.stop_async();
    };

    end_point ep("0.0.0.0", PORT);
    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = on_data;
    cb.OnDisconnected         = on_disconn;

    tcp_server l;
    l.start({ep}, cb);

    std::jthread th = std::jthread([]() {
        using boost::asio::ip::tcp;

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", std::to_string(PORT));

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        boost::system::error_code error;
        std::array<char, 5> buf_write = {'H', 'E', 'L', 'L', 'O'};
        socket.write_some(boost::asio::buffer(buf_write), error);

        for (;;) {
            std::array<char, 5> buf_read;

            socket.read_some(boost::asio::buffer(buf_read), error);
            if (buf_read == buf_write)
                break;
            if (error)
                throw boost::system::system_error(error);
        }
    });

    REQUIRE_NOTHROW(th.join());
    l.future_get_once();
}

TEST_CASE("tcp_server: check reaction to client immediate disconnection", "[sg::net::tcp_server]") {
    using namespace sg::net;

    std::atomic_bool boolCon{false};
    std::atomic_bool boolDis{false};

    tcp_server::session_created_cb_t on_conn = [&](tcp_server&, tcp_server::session_id_t) {
        boolCon = true;
    };
    tcp_server::session_disconnected_cb_t on_disconn = [&](tcp_server& l, tcp_server::session_id_t,
                                                           std::exception_ptr) {
        boolDis = true;
        l.stop_async();
    };

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = on_conn;
    cb.OnDisconnected   = on_disconn;

    end_point ep("0.0.0.0", PORT);

    tcp_server l;
    l.start({ep}, cb);

    std::jthread th = std::jthread([]() {
        using boost::asio::ip::tcp;

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", std::to_string(PORT));

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);
    });

    REQUIRE_NOTHROW(th.join());
    l.future_get_once();

    REQUIRE(boolCon == true);
    REQUIRE(boolDis == true);
}

TEST_CASE("tcp_server: check dropping tcp_server drops all connections", "[sg::net::tcp_server]") {
    for (int i = 0; i < 100; i++) {
        using namespace sg::net;

        std::binary_semaphore sem{0};
        std::atomic_int stop_count{0};

        tcp_server::session_created_cb_t onConn = [&](tcp_server&, tcp_server::session_id_t) {
            sem.release();
        };
        tcp_server::stopped_listening_cb_t onStop = [&](tcp_server&) { stop_count++; };

        end_point ep("0.0.0.0", PORT);
        std::jthread th;

        {
            tcp_server::CallBacks cb;
            cb.OnStoppedListening = onStop;
            cb.OnSessionCreated   = onConn;

            tcp_server l;
            l.start({ep}, cb);

            th = std::jthread([]() {
                using boost::asio::ip::tcp;

                boost::asio::io_context io_context;

                tcp::resolver resolver(io_context);
                tcp::resolver::results_type endpoints =
                    resolver.resolve("127.0.0.1", std::to_string(PORT));

                tcp::socket socket(io_context);
                boost::asio::connect(socket, endpoints);

                if (!socket.is_open())
                    return;
            });

            /* at least echo once */
            sem.acquire();
        }

        // Check client disconnected
        REQUIRE_NOTHROW(th.join());
        REQUIRE(stop_count == 1);
    }
}

TEST_CASE("tcp_server: check stop_async() drops all connections", "[sg::net::tcp_server]") {
    using namespace sg::net;

    std::binary_semaphore sem{0};
    std::atomic_int stop_count{0};

    tcp_server::session_created_cb_t onConn = [&](tcp_server&, tcp_server::session_id_t) {
        sem.release();
    };
    tcp_server::stopped_listening_cb_t onStop = [&](tcp_server&) { stop_count++; };

    end_point ep("0.0.0.0", PORT);
    std::jthread th;

    tcp_server::CallBacks cb;
    cb.OnStoppedListening = onStop;
    cb.OnSessionCreated   = onConn;

    tcp_server l;
    l.start({ep}, cb);

    th = std::jthread([]() {
        using boost::asio::ip::tcp;

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", std::to_string(PORT));

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        if (!socket.is_open())
            return;
    });

    /* at least echo once */
    sem.acquire();

    l.stop_async();
    l.future_get_once();

    // Check client disconnected
    REQUIRE_NOTHROW(th.join());
    REQUIRE(stop_count == 1);
}

TEST_CASE("tcp_server: check destructor works if start(...) not started", "[sg::net::tcp_server]") {
    using namespace sg::net;

    { tcp_server l; }
}

TEST_CASE("tcp_server: set_keepalive(...)", "[sg::net::tcp_server]") {
    using namespace sg::net;
    end_point ep("0.0.0.0", PORT);

    tcp_server server;
    server.start({ep}, tcp_server::CallBacks());

    server.set_keepalive(keepalive_t{});

    unsigned outsideRange = (unsigned)std::numeric_limits<int>::max() + 1;

    // Check range errors
    REQUIRE_THROWS(server.set_keepalive(keepalive_t{
    .enable = true, .idle_seconds = outsideRange, .interval_seconds = 1, .count = 5}));
    REQUIRE_THROWS(server.set_keepalive(keepalive_t{
        .enable = true, .idle_seconds = 1, .interval_seconds = outsideRange, .count = 5}));
    REQUIRE_THROWS(server.set_keepalive(keepalive_t{
        .enable = true, .idle_seconds = 1, .interval_seconds = 1, .count = outsideRange}));
}

TEST_CASE("tcp_server: set_timeout(...)", "[sg::net::tcp_server]") {
    for (int i = 0; i < 100; i++) {
        using namespace sg::net;
        end_point ep("0.0.0.0", PORT);

        tcp_server server;
        server.start({ep}, tcp_server::CallBacks());

        server.set_timeout(true);
    }
}

TEST_CASE("tcp_server: you can't listen to same port twice", "[sg::net::tcp_server]") {
    using namespace sg::net;

    // wild-card address
    {
        end_point ep("0.0.0.0", PORT);

        tcp_server server1;
        tcp_server server2;
        server1.start({ep}, {});
        REQUIRE_THROWS(server2.start({ep}, {}));
    }

    // specific address
    {
        end_point ep("127.0.0.1", PORT);

        tcp_server server1;
        tcp_server server2;
        server1.start({ep}, {});
        REQUIRE_THROWS(server2.start({ep}, {}));
    }

    // mix
    {
        end_point ep1("0.0.0.0", PORT);
        end_point ep2("127.0.0.1", PORT);

        tcp_server server1;
        tcp_server server2;
        server1.start({ep1}, {});
        REQUIRE_THROWS(server2.start({ep2}, {}));
    }

    // mix
    {
        end_point ep1("127.0.0.1", PORT);
        end_point ep2("0.0.0.0", PORT);

        tcp_server server1;
        tcp_server server2;
        server1.start({ep1}, {});
        REQUIRE_THROWS(server2.start({ep2}, {}));
    }

}

TEST_CASE("tcp_server: allow for disconnection in OnSessionCreated() callbacks", "[sg::net::tcp_server]") {
    using namespace sg::net;

    end_point ep("127.0.0.1", PORT);

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server& l, tcp_server::session_id_t id) { l.disconnect(id); };

    tcp_server l;
    l.start({ep}, cb);

    for (auto i = 0; i< 10; ++i)
    {
        tcp_client client;
        client.connect(ep, nullptr, nullptr);
        client.session().wait_until_stopped();
    }
}

TEST_CASE("tcp_server: multiple connections", "[sg::net::tcp_server]") {
    using namespace sg::net;

    end_point ep("127.0.0.1", PORT);

    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = [](tcp_server& l, tcp_server::session_id_t id,
                                   const std::byte* data, size_t length) {
        l.session(id)->write(data, length);
    };

    tcp_server l;
    l.start({ep}, cb);

    // all connected
    std::vector<std::shared_ptr<tcp_client_sync>> clients;
    for (auto i = 0; i< 150; ++i) {
        auto client = std::make_shared<tcp_client_sync>();
        client->connect(ep);
        clients.push_back(client);
        client->write(fmt::format("{}\n", i));
    }

    for (auto i = 0; i< 150; ++i)
        REQUIRE(clients[i]->read_until("\n") == fmt::format("{}\n", i));
}

TEST_CASE("tcp_server: proxy simulation", "[sg::net::tcp_server]") {
    using namespace sg::net;

    std::binary_semaphore sessConn{0};

    end_point main_ep("127.0.0.1", PORT);
    end_point proxy_ep("127.0.0.1", PORT+1);

    tcp_server server_top;
    tcp_client client_intermediate;
    tcp_server proxy_server;
    {
        tcp_server::CallBacks cb;
        cb.OnSessionDataAvailable =
            [](tcp_server& l, tcp_server::session_id_t id, const std::byte* data, size_t length) {
                l.session(id)->write(data, length);
        };
        server_top.start({main_ep}, cb);
    }

    {
        tcp_session::on_data_available_cb_t onDataAvailable =
            [&](tcp_session&, const std::byte* data, size_t length) {
                for (const auto& sess : proxy_server.sessions() | std::views::values)
                    sess->write(data, length);
            };
        client_intermediate.connect(main_ep, onDataAvailable, nullptr);
    }

    /* proxy server:
     *  - if you get message from clients, forward to intermediate client
     */
    {
        tcp_server::CallBacks cb;
        cb.OnSessionDataAvailable =
            [&](tcp_server&, tcp_server::session_id_t, const std::byte* data, size_t length) {
                client_intermediate.session().write(data, length);
        };
        cb.OnSessionCreated = [&](tcp_server&, tcp_server::session_id_t) {
            sessConn.release();
        };
        proxy_server.start({proxy_ep}, cb);
    }


    for (auto i = 0; i< 100; ++i)
    {
        tcp_client_sync client;
        tcp_client_sync client2;
        client.connect(proxy_ep);
        sessConn.acquire();
        client2.connect(proxy_ep);
        sessConn.acquire(); // make sure that server session is setup (as we write on client and read
                            // from another)

        client.write("Dasdas\n");
        std::ignore = client.read_until("\n");
        std::ignore = client2.read_until("\n");

        client2.write("Dasdas\n");
        std::ignore = client.read_until("\n");
        std::ignore = client2.read_until("\n");
    }


}
