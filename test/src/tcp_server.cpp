#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "helpers.h"

#include "sg/net/tcp_client.h"
#include "sg/net/tcp_client_sync.h"
#include "sg/net/tcp_server.h"

#include "sg/jthread.h"
#include "sg/random.h"

#include <boost/asio.hpp>
#include <fmt/format.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <random>
#include <stdexcept>
#include <semaphore>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace sg::net;
static port_t PORT = 4444; // 55555 can't be used on macOS!


TEST_CASE("tcp_server: check bad endpoint throws exception during start()", "[sg::net::tcp_server]") {
    end_point ep;
    ep.port = PORT;
    ep.ip = "8.8.8.8";

    tcp_server l;
    REQUIRE_THROWS(l.start({ep}, {}));
}

TEST_CASE("tcp_server: check empty endpoint list throws during start()", "[sg::net::tcp_server]") {
    tcp_server l;
    REQUIRE_THROWS_AS(l.start({}, {}), std::invalid_argument);

    // start() must have left the server un-started, so a subsequent valid start works.
    end_point ep("127.0.0.1", PORT);
    REQUIRE_NOTHROW(l.start({ep}, {}));
}

TEST_CASE("tcp_server: check start/stop callback", "[sg::net::tcp_server]") {
    std::atomic_int stop_count{0};
    std::binary_semaphore start_sem{0};

    using namespace sg::net;

    std::atomic_bool stopped_with_error{false};

    tcp_server::started_listening_cb_t onStart = [&](tcp_server&) { start_sem.release(); };
    tcp_server::stopped_listening_cb_t onStop  = [&](tcp_server&, std::exception_ptr ex) {
        stopped_with_error.store(ex != nullptr);
        stop_count++;
    };

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
    REQUIRE_FALSE(stopped_with_error.load());
    REQUIRE(l.last_error() == nullptr);
}

struct tcp_server_test0 {
    std::atomic_int stop_count{0};
    std::binary_semaphore start_sem{0};

    tcp_server l;
    void start() {
        end_point ep("127.0.0.1", PORT);
        auto onstart = std::bind(&tcp_server_test0::on_start, this, std::placeholders::_1);
        auto onstop =
            std::bind(&tcp_server_test0::on_stop, this, std::placeholders::_1, std::placeholders::_2);

        tcp_server::CallBacks cb;
        cb.OnStartedListening = onstart;
        cb.OnStoppedListening = onstop;
        l.start({ep}, cb);
    }
    void on_start(tcp_server&) { start_sem.release(); }
    void on_stop(tcp_server&, std::exception_ptr) { stop_count++; }
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

        auto buf_write = sg::random::generate<char>(20);
        boost::asio::write(socket, boost::asio::buffer(buf_write));

        std::vector<char> buf_read(20);
        boost::asio::read(socket, boost::asio::buffer(buf_read));

        if (buf_write != buf_read)
            throw std::runtime_error("error");

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

// TEST_CASE("tcp_server stopped_listening_cb_t cb exception handling", "[sg::net::tcp_server]") {
//     using namespace sg::net;
//
//     tcp_server::stopped_listening_cb_t onStop = [](tcp_server&, std::exception_ptr) {
//         throw std::runtime_error("bad error!");
//     };
//
//     tcp_server::CallBacks cb;
//     cb.OnStoppedListening = onStop;
//
//     end_point ep("0.0.0.0", PORT);
//     auto l = tcp_server();
//     l.start({ep}, cb);
//     l.stop_async();
//     REQUIRE_THROWS(l.future_get_once());
// }

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

    bool allMatch = false;

    tcp_server::session_data_available_cb_t on_data = [&](tcp_server& l, tcp_server::session_id_t id,
                                                         const std::byte* dat, size_t size) {
        auto w = sg::make_shared_c_buffer<std::byte>(size);
        std::memcpy(w.get(), dat, size);

        auto local  = l.session(id)->local_endpoint();
        auto remote = l.session(id)->remote_endpoint();
        allMatch = (local.ip == remote.ip) && (local.port == PORT);

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

    REQUIRE(allMatch);
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
    using namespace sg::net;

    size_t count = 100;
    std::vector<std::jthread> threads;

    {
        std::atomic_int connected{0};
        std::binary_semaphore sem{0};

        tcp_server::session_created_cb_t onConn = [&](tcp_server&, tcp_server::session_id_t) {
            if (++connected == 100)
                sem.release();
        };

        tcp_server::CallBacks cb;
        cb.OnSessionCreated   = onConn;

        tcp_server l;
        l.start({{"127.0.0.1", PORT}}, cb);

        for (size_t i = 0; i < count; i++) {
            auto th = std::jthread([]() {
                using boost::asio::ip::tcp;

                boost::asio::io_context io_context;

                tcp::resolver resolver(io_context);
                tcp::resolver::results_type endpoints =
                    resolver.resolve("127.0.0.1", std::to_string(PORT));

                tcp::socket socket(io_context);
                boost::asio::connect(socket, endpoints);

                std::string msg ="dasd";
                try {
                    while (true) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        socket.write_some(boost::asio::buffer(msg));
                    }
                } catch (...){};

            });

            threads.emplace_back(std::move(th));
        }

        sem.acquire();
    }

    for (auto& th : threads)
        th.join();
}

TEST_CASE("tcp_server: check stop_async() drops all connections", "[sg::net::tcp_server]") {
    using namespace sg::net;

    std::binary_semaphore sem{0};
    std::atomic_int stop_count{0};

    tcp_server::session_created_cb_t onConn = [&](tcp_server&, tcp_server::session_id_t) {
        sem.release();
    };
    tcp_server::stopped_listening_cb_t onStop = [&](tcp_server&, std::exception_ptr) {
        stop_count++;
    };

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

TEST_CASE("tcp_server: check the same server can be restarted after stop", "[sg::net::tcp_server]") {
    using namespace sg::net;

    end_point ep("127.0.0.1", PORT);

    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = [](tcp_server& l, tcp_server::session_id_t id,
                                   const std::byte* data, size_t length) {
        l.session(id)->write(data, length);
    };

    tcp_server l;

    for (auto round = 0; round < 3; ++round) {
        l.start({ep}, cb);

        tcp_client_sync client;
        client.connect(ep);
        client.write(fmt::format("round{}\n", round));
        REQUIRE(client.read_until("\n") == fmt::format("round{}\n", round));

        l.stop_async();
        l.future_get_once();
    }
}

#if !defined(__APPLE__)
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
#endif

TEST_CASE("tcp_server: allow for disconnection in OnSessionCreated() callbacks", "[sg::net::tcp_server]") {
    using namespace sg::net;

    end_point ep("127.0.0.1", PORT);

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server& l, tcp_server::session_id_t id) {
        l.disconnect(id);
    };

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
    std::counting_semaphore<2> sessDisc{0};

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
        tcp_session::Callbacks::OnDataAvailable onDataAvailable =
            [&](tcp_session&, const std::byte* data, size_t length) {
                for (const auto& sess : proxy_server.sessions() | std::views::values)
                    try {
                        sess->write(data, length);
                    } catch (...) {
                        /* the session may be a disconnected one that is not yet removed from
                         * proxy_server's session list (OnDisconnected fires before the removal);
                         * letting the exception escape would kill this session's reader and with
                         * it the whole relay */
                    }
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
        cb.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
            sessDisc.release();
        };
        proxy_server.start({proxy_ep}, cb);
    }


    for (auto i = 0; i< 100; ++i)
    {
        {
            tcp_client_sync client;
            tcp_client_sync client2;
            client.connect(proxy_ep);
            sessConn.acquire();
            client2.connect(proxy_ep);
            sessConn.acquire();

            client.write("Dasdas\n");
            std::ignore = client.read_until("\n");
            std::ignore = client2.read_until("\n");

            client2.write("Dasdas\n");
            std::ignore = client.read_until("\n");
            std::ignore = client2.read_until("\n");
        }

        sessDisc.acquire();
        sessDisc.acquire();
    }
}

TEST_CASE("tcp_server: check that disconnect callback is called if connect callback throws", "[sg::net::tcp_server]") {
    tcp_server::CallBacks cbs;
    std::binary_semaphore discCalled{0};

    cbs.OnSessionCreated = [](tcp_server&, tcp_server::session_id_t) {
        throw std::runtime_error("test");
    };
    cbs.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        discCalled.release();
    };

    tcp_server l;
    l.start({{"127.0.0.1", PORT}}, cbs);

    tcp_client client;
    client.connect({"127.0.0.1", PORT}, nullptr, nullptr);

    discCalled.acquire();
}

TEST_CASE("tcp_server: echo works across a range of options_t::no_threads", "[sg::net::tcp_server]") {
    using namespace sg::net;

    auto no_threads = GENERATE(size_t{1}, size_t{2}, size_t{4}, size_t{8}, size_t{16});
    CAPTURE(no_threads);

    constexpr int client_count = 50;

    end_point ep("127.0.0.1", PORT);

    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = [](tcp_server& l, tcp_server::session_id_t id,
                                   const std::byte* data, size_t length) {
        l.session(id)->write(data, length);
    };

    tcp_server::options_t opts;
    opts.no_threads = no_threads;

    tcp_server l;
    l.start({ep}, cb, opts);

    std::vector<std::shared_ptr<tcp_client_sync>> clients;
    clients.reserve(client_count);
    for (auto i = 0; i < client_count; ++i) {
        auto client = std::make_shared<tcp_client_sync>();
        client->connect(ep);
        clients.push_back(client);
        client->write(fmt::format("{}\n", i));
    }

    for (auto i = 0; i < client_count; ++i)
        REQUIRE(clients[i]->read_until("\n") == fmt::format("{}\n", i));

    l.stop_async();
    l.future_get_once();
}


TEST_CASE("tcp_server: check future_get_once() on not-running server","[sg::net::tcp_server]") {
    tcp_server s;
    s.future_get_once();
}

TEST_CASE("tcp_server: throwing OnDisconnected releases the session (peer disconnects)",
          "[sg::net::tcp_server]") {
    scoped_deadline watchdog(
        "DEADLOCK: throwing OnDisconnected did not release the session (peer disconnects)");

    end_point ep("127.0.0.1", PORT);

    std::binary_semaphore disconnected{0};
    tcp_server::CallBacks cb;
    cb.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        disconnected.release();
        throw std::runtime_error("boom (this is expected, ignore)");
    };

    tcp_server l;
    l.start({ep}, cb);

    {
        tcp_client client;
        client.connect(ep, nullptr, nullptr);
        client.disconnect();
    }

    disconnected.acquire();

    /* stop_async() waits for the active-session count to drop to zero, and that
     * decrement is what the escaping exception used to skip */
    l.stop_async();
    l.future_get_once();

    REQUIRE(l.is_stopped());
    REQUIRE(l.clients_count() == 0);
}

TEST_CASE("tcp_server: throwing OnDisconnected releases the session (server disconnects)",
          "[sg::net::tcp_server]") {
    scoped_deadline watchdog(
        "DEADLOCK: throwing OnDisconnected did not release the session (server disconnects)");

    end_point ep("127.0.0.1", PORT);

    std::binary_semaphore disconnected{0};
    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = [](tcp_server& s, tcp_server::session_id_t id, const std::byte*,
                                   size_t) { s.disconnect(id); };
    cb.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        disconnected.release();
        throw std::runtime_error("boom (this is expected, ignore)");
    };

    tcp_server l;
    l.start({ep}, cb);

    tcp_client_sync client;
    client.connect(ep);
    client.write("hello\n");

    disconnected.acquire();

    l.stop_async();
    l.future_get_once();

    REQUIRE(l.is_stopped());
    REQUIRE(l.clients_count() == 0);
}

TEST_CASE("tcp_server: OnDisconnected throwing a non-std exception releases the session",
          "[sg::net::tcp_server]") {
    scoped_deadline watchdog(
        "DEADLOCK: OnDisconnected throwing a non-std exception did not release the session");

    end_point ep("127.0.0.1", PORT);

    std::binary_semaphore disconnected{0};
    tcp_server::CallBacks cb;
    cb.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        disconnected.release();
        throw 42; // not derived from std::exception
    };

    tcp_server l;
    l.start({ep}, cb);

    {
        tcp_client client;
        client.connect(ep, nullptr, nullptr);
        client.disconnect();
    }

    disconnected.acquire();

    l.stop_async();
    l.future_get_once();

    REQUIRE(l.is_stopped());
    REQUIRE(l.clients_count() == 0);
}

TEST_CASE("tcp_server: throwing OnDisconnected does not stall stop_async()",
          "[sg::net::tcp_server]") {
    scoped_deadline watchdog("DEADLOCK: stop_async() stalled on a throwing OnDisconnected");

    constexpr int count = 5;
    end_point ep("127.0.0.1", PORT);

    std::counting_semaphore<count> connected{0};
    std::atomic_int disconnections{0};

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server&, tcp_server::session_id_t) { connected.release(); };
    cb.OnDisconnected   = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        ++disconnections;
        throw std::runtime_error("boom (this is expected, ignore)");
    };

    tcp_server l;
    l.start({ep}, cb);

    /* hold every connection open, so that all sessions are torn down by
     * stop_async() itself */
    std::vector<std::unique_ptr<tcp_client_sync>> clients;
    for (auto i = 0; i < count; ++i) {
        auto client = std::make_unique<tcp_client_sync>();
        client->connect(ep);
        clients.push_back(std::move(client));
        connected.acquire();
    }

    l.stop_async();
    l.future_get_once();

    REQUIRE(l.is_stopped());
    REQUIRE(l.clients_count() == 0);
    REQUIRE(disconnections == count);
}

TEST_CASE("tcp_server: throwing OnDisconnected does not hang the destructor",
          "[sg::net::tcp_server]") {
    scoped_deadline watchdog("DEADLOCK: ~tcp_server() hung on a throwing OnDisconnected");

    end_point ep("127.0.0.1", PORT);

    std::binary_semaphore connected{0};
    std::atomic_int disconnections{0};

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server&, tcp_server::session_id_t) { connected.release(); };
    cb.OnDisconnected   = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        ++disconnections;
        throw std::runtime_error("boom (this is expected, ignore)");
    };

    /* the client outlives the server, so the session is still live when the
     * server is destructed */
    tcp_client_sync client;
    {
        tcp_server l;
        l.start({ep}, cb);

        client.connect(ep);
        connected.acquire();
    }

    REQUIRE(disconnections == 1);
}

TEST_CASE("tcp_server: can be restarted after a throwing OnDisconnected",
          "[sg::net::tcp_server]") {
    scoped_deadline watchdog("DEADLOCK: restart after a throwing OnDisconnected did not complete");

    end_point ep("127.0.0.1", PORT);

    std::binary_semaphore disconnected{0};
    tcp_server::CallBacks cb;
    cb.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        disconnected.release();
        throw std::runtime_error("boom (this is expected, ignore)");
    };

    tcp_server l;

    /* a leaked session or a stale active-session count would break the next
     * start(), which requires both to have been reset */
    for (auto round = 0; round < 3; ++round) {
        l.start({ep}, cb);

        {
            tcp_client client;
            client.connect(ep, nullptr, nullptr);
            client.disconnect();
        }

        disconnected.acquire();

        l.stop_async();
        l.future_get_once();

        REQUIRE(l.is_stopped());
        REQUIRE(l.clients_count() == 0);
    }
}

// ---------------------------------------------------------------------------
// Run this ideally under ThreadSanitizer,:
//
//   cmake -DSANITIZE=ON -DSANITIZE_THREAD=ON
//
// It tests concurrentcy on on a pool of hardware_concurrency() io threads:
//   * streamers  -> sustained, pipelined traffic => a read and a write are in
//                   flight on the same session at once (per-session strand);
//   * churners   -> connect/echo/disconnect in a loop => accept racing the
//                   close() posted by stop_async() (per-acceptor strand), plus
//                   a connection storm overlapping teardown;
//   * stop is issued MID-FLIGHT, so teardown happens under load.
//
// A watchdog aborts (with a message) if teardown exceeds a deadline, so a
// regression of the stop_async() deadlock fails loudly instead of hanging.
// Worker threads never call Catch2 macros (they are not thread-safe); they only
// set atomics, which the main thread asserts on after joining.
// ---------------------------------------------------------------------------
TEST_CASE("tcp_server: multi-threaded stress (strands + teardown under load)",
          "[.][sg::net::tcp_server]") {
    using namespace std::chrono_literals;

    constexpr int kIterations = 25;
    constexpr int kStreamers  = 16;
    constexpr int kChurners   = 16;
    constexpr int kBatch      = 8;   // messages pipelined per streamer round
    const size_t  kThreads    = std::max<unsigned>(2, std::thread::hardware_concurrency());
    const port_t  kBasePort   = 4600;

    std::mt19937 rng(std::random_device{}());
    std::atomic<long> total_roundtrips{0}; // guards against the test doing nothing

    for (int it = 0; it < kIterations; ++it) {
        const port_t port = static_cast<port_t>(kBasePort + (it % 50));
        end_point ep("127.0.0.1", port);

        tcp_server::CallBacks cb;
        cb.OnSessionDataAvailable = [](tcp_server& s, tcp_server::session_id_t id,
                                       const std::byte* data, size_t len) {
            // Echo back on the same session; tolerate a session that is going away.
            try { s.session(id)->write(data, len); } catch (...) {}
        };

        tcp_server::options_t opts;
        opts.no_threads = kThreads;

        tcp_server server;
        server.start({ep}, cb, opts);

        std::atomic<bool> stop_clients{false};
        std::atomic<bool> corruption{false};
        std::vector<std::thread> clients;
        clients.reserve(kStreamers + kChurners);

        // Streamers: one long-lived connection, pipelined batches.
        for (int c = 0; c < kStreamers; ++c) {
            clients.emplace_back([&, c]() {
                try {
                    tcp_client_sync client;
                    tcp_session::options_t sopts;
                    sopts.timeout_msec = 2000;
                    client.connect(ep, sopts);

                    long n = 0;
                    while (!stop_clients.load(std::memory_order_relaxed)) {
                        std::vector<std::string> sent;
                        sent.reserve(kBatch);
                        for (int b = 0; b < kBatch; ++b) {
                            auto msg = fmt::format("s{}-{}\n", c, n++);
                            client.write(msg);
                            sent.push_back(std::move(msg));
                        }
                        for (auto& msg : sent) {
                            if (client.read_until("\n") != msg) {
                                corruption.store(true, std::memory_order_relaxed);
                                return;
                            }
                            total_roundtrips.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                } catch (...) {
                    // Expected: the server stops mid-stream / a read times out.
                }
            });
        }

        // Churners: connect/echo/disconnect in a tight loop.
        for (int c = 0; c < kChurners; ++c) {
            clients.emplace_back([&, c]() {
                long n = 0;
                while (!stop_clients.load(std::memory_order_relaxed)) {
                    try {
                        tcp_client_sync client;
                        tcp_session::options_t sopts;
                        sopts.timeout_msec = 2000;
                        client.connect(ep, sopts);
                        auto msg = fmt::format("k{}-{}\n", c, n++);
                        client.write(msg);
                        if (client.read_until("\n") != msg)
                            corruption.store(true, std::memory_order_relaxed);
                        else
                            total_roundtrips.fetch_add(1, std::memory_order_relaxed);
                    } catch (...) {
                        // Expected once the server begins shutting down.
                    }
                }
            });
        }

        // Let traffic ramp up, then stop MID-FLIGHT after a randomised delay.
        std::this_thread::sleep_for(
            std::chrono::milliseconds(std::uniform_int_distribution<int>(2, 40)(rng)));

        // --- watchdog-guarded teardown ---
        std::atomic<bool> teardown_done{false};
        std::thread watchdog([&]() {
            const auto deadline = std::chrono::steady_clock::now() + 60s;
            while (std::chrono::steady_clock::now() < deadline) {
                if (teardown_done.load(std::memory_order_acquire))
                    return;
                std::this_thread::sleep_for(50ms);
            }
            if (!teardown_done.load(std::memory_order_acquire)) {
                std::fprintf(stderr,
                             "\n[STRESS] DEADLOCK: teardown exceeded 60s "
                             "(iteration %d, no_threads=%zu)\n", it, kThreads);
                std::fflush(stderr);
                std::abort();
            }
        });

        server.stop_async();
        server.future_get_once();   // hangs here if stop_async() deadlocks

        stop_clients.store(true, std::memory_order_relaxed);
        for (auto& t : clients)
            t.join();

        teardown_done.store(true, std::memory_order_release);
        watchdog.join();

        REQUIRE_FALSE(corruption.load());
        REQUIRE(server.is_stopped());
        REQUIRE(server.clients_count() == 0);
    }

    // Make sure the run actually exercised the server (e.g. connects succeeded).
    REQUIRE(total_roundtrips.load() > 0);
}

TEST_CASE("tcp_server: check a server listening on several endpoints", "[sg::net::tcp_server]") {
    scoped_deadline watchdog("DEADLOCK: multi-endpoint stop_async() stalled");

    using namespace sg::net;

    end_point ep1("127.0.0.1", PORT);
    end_point ep2("127.0.0.1", static_cast<port_t>(PORT + 1));

    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = [](tcp_server& l, tcp_server::session_id_t id,
                                   const std::byte* data, size_t length) {
        l.session(id)->write(data, length);
    };

    tcp_server l;
    l.start({ep1, ep2}, cb);

    /* each endpoint gets an acceptor of its own, and each acceptor a listener and a backoff
     * timer of its own; all of them must serve traffic, and all of them must stop */
    for (const auto& ep : {ep1, ep2}) {
        tcp_client_sync client;
        client.connect(ep);
        client.write("hello\n");
        REQUIRE(client.read_until("\n") == "hello\n");
    }

    l.stop_async();
    l.future_get_once();

    REQUIRE(l.is_stopped());
    REQUIRE(l.last_error() == nullptr);
}

TEST_CASE("tcp_server: callback threads are recognised, and waiting from one throws",
          "[sg::net::tcp_server]") {
    scoped_deadline watchdog("DEADLOCK: future_get_once() from a callback did not return");

    using namespace sg::net;

    /* Every callback below runs on a thread the shutdown sequence has to wait for: the I/O
     * workers, the pool thread behind OnDisconnected, and the io_pool's stopped-callback
     * thread. Waiting for that shutdown from inside one can never complete, so it must be
     * reported rather than hung. ~tcp_server has the same restriction, but can only terminate
     * on breach, so it is the predicate that is pinned here. */
    std::atomic_int created_flag{-1}, data_flag{-1}, disconnected_flag{-1}, stopped_flag{-1};
    std::atomic_int threw{0};
    std::binary_semaphore stopped_sem{0};

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server& l, tcp_server::session_id_t) {
        created_flag.store(l.running_in_callback_thread() ? 1 : 0);
        try {
            l.future_get_once();
        } catch (const std::logic_error&) {
            ++threw;
        }
    };
    cb.OnSessionDataAvailable = [&](tcp_server& l, tcp_server::session_id_t id,
                                    const std::byte* data, size_t length) {
        data_flag.store(l.running_in_callback_thread() ? 1 : 0);
        l.session(id)->write(data, length);
    };
    cb.OnDisconnected = [&](tcp_server& l, tcp_server::session_id_t, std::exception_ptr) {
        disconnected_flag.store(l.running_in_callback_thread() ? 1 : 0);
    };
    cb.OnStoppedListening = [&](tcp_server& l, std::exception_ptr) {
        stopped_flag.store(l.running_in_callback_thread() ? 1 : 0);
        stopped_sem.release();
    };

    tcp_server l;
    l.start({end_point("127.0.0.1", PORT)}, cb);

    /* the caller's own thread is not a callback thread */
    REQUIRE_FALSE(l.running_in_callback_thread());

    {
        tcp_client_sync client;
        client.connect(end_point("127.0.0.1", PORT));
        client.write("hello\n");
        REQUIRE(client.read_until("\n") == "hello\n");
    }

    l.stop_async();
    l.future_get_once();          // legitimate: not on a callback thread
    stopped_sem.acquire();

    REQUIRE(created_flag.load() == 1);
    REQUIRE(data_flag.load() == 1);
    REQUIRE(disconnected_flag.load() == 1);
    REQUIRE(stopped_flag.load() == 1);
    REQUIRE(threw.load() == 1);   // future_get_once() from a callback throws, never hangs

    REQUIRE(l.is_stopped());
}

TEST_CASE("tcp_server/tcp_session: options_t accepts designated initialisers",
          "[sg::net::tcp_server]") {
    /* Both options_t structs must stay aggregates. These are compile-time guarantees as much as
     * runtime ones: if either regains a constructor -- e.g. the old clang-36032 workaround is
     * reinstated -- the designated initialisers below stop compiling. */
    static_assert(std::is_aggregate_v<tcp_server::options_t>);
    static_assert(std::is_aggregate_v<tcp_session::options_t>);

    end_point ep("127.0.0.1", PORT);

    tcp_server server;
    server.start({ep}, {}, {.no_threads = 2});

    tcp_client client;
    client.connect(ep, nullptr, nullptr, {.timeout_msec = 500});

    REQUIRE(client.is_connected());
}

TEST_CASE("tcp_server: a session that fails setup before onConnected is never registered",
          "[sg::net::tcp_server]") {
    /* tcp_session pairs its connected and disconnected callbacks: a session that dies before
     * onConnected() gets no onDisconnected(), and so never reaches on_session_stopped(). The
     * server must therefore not have registered it -- registering around start() instead left
     * such a session in m_sessions with m_active_sessions above zero, and stop() then waited on
     * it for ever. */
    scoped_deadline watchdog(
        "DEADLOCK: a session that failed setup was left registered and stop() never completed");

    end_point ep("127.0.0.1", PORT);

    std::atomic_int created{0};
    std::atomic_int disconnected{0};
    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server&, tcp_server::session_id_t) { ++created; };
    cb.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        ++disconnected;
    };

    /* set_keepalive() rejects an idle time above INT_MAX, so session setup is guaranteed to fail
     * before start() reaches onConnected() -- the same shape as a peer that resets between
     * accept() and the socket options being applied, but deterministic */
    tcp_server::options_t options;
    options.session_options.keepalive.idle_seconds = std::numeric_limits<unsigned>::max();

    tcp_server server;
    server.start({ep}, cb, options);

    {
        boost::asio::io_context context;
        boost::asio::ip::tcp::socket socket(context);
        socket.connect(
            boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(ep.ip), ep.port));

        /* the session is torn down asynchronously, so give the server time to get it wrong */
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        REQUIRE(created == 0);
        REQUIRE(disconnected == 0);
        REQUIRE(server.clients_count() == 0);
    }

    // hangs if the failed session is still counted in m_active_sessions
    server.stop_async();
    server.future_get_once();
}
