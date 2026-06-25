#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "sg/tcp_client.h"
#include "sg/tcp_client_sync.h"
#include "sg/jthread.h"
#include "sg/random.h"
#include "sg/tcp_server.h"

#include <boost/asio.hpp>
#include <fmt/format.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <semaphore>
#include <string>
#include <thread>
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
//     tcp_server::stopped_listening_cb_t onStop = [](tcp_server&) {
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

// ---------------------------------------------------------------------------
// Run this ideally under ThreadSanitizer, which:
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
          "[sg::net::tcp_server]") {
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
