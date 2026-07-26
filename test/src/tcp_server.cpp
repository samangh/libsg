#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "sg/net/tcp_client.h"
#include "sg/net/tcp_client_sync.h"
#include "sg/net/tcp_native.h"
#include "sg/net/tcp_server.h"

#include "sg/jthread.h"
#include "sg/random.h"

#include <boost/asio.hpp>
#include <fmt/format.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <mutex>
#include <random>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

using namespace sg::net;
static port_t PORT = 4444; // 55555 can't be used on macOS!

TEST_CASE("tcp_server: check bad endpoint throws exception during launch()", "[sg::net::tcp_server]") {
    end_point ep;
    ep.port = PORT;
    ep.ip = "8.8.8.8";

    REQUIRE_THROWS(tcp_server::launch({ep}, {}));
}

TEST_CASE("tcp_server: check empty endpoint list throws during launch()", "[sg::net::tcp_server]") {
    REQUIRE_THROWS_AS(tcp_server::launch({}, {}), std::invalid_argument);

    // a failed launch() must not have bound anything, so a subsequent valid launch works.
    end_point ep("127.0.0.1", PORT);
    REQUIRE_NOTHROW(tcp_server::launch({ep}, {}));
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

    auto l = tcp_server::launch({ep}, cb);

    start_sem.acquire();
    REQUIRE(stop_count == 0);

    l->stop_async();
    l->wait_until_stopped();
    REQUIRE(stop_count == 1);
}

struct tcp_server_test0 {
    std::atomic_int stop_count{0};
    std::binary_semaphore start_sem{0};

    std::unique_ptr<tcp_server> l;
    void start() {
        end_point ep("127.0.0.1", PORT);
        auto onstart = std::bind(&tcp_server_test0::on_start, this, std::placeholders::_1);
        auto onstop  = std::bind(&tcp_server_test0::on_stop, this, std::placeholders::_1);

        tcp_server::CallBacks cb;
        cb.OnStartedListening = onstart;
        cb.OnStoppedListening = onstop;
        l = tcp_server::launch({ep}, cb);
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

    t.l->stop_async();
    t.l->wait_until_stopped();
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

    auto l = tcp_server::launch({ep}, cb);

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

    l->wait_until_stopped();

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

    auto l = tcp_server::launch({ep}, cb);

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

    auto l = tcp_server::launch({ep}, cb);

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

    l->wait_until_stopped();
    REQUIRE(has_exception);
}

TEST_CASE("tcp_server started_listening_cb_t exception handling", "[sg::net::tcp_server]") {
    using namespace sg::net;

    tcp_server::started_listening_cb_t onListening = [](tcp_server&) {
        throw std::runtime_error("bad error!");
    };

    end_point ep("0.0.0.0", PORT);

    tcp_server::CallBacks cb;
    cb.OnStartedListening = onListening;

    REQUIRE_THROWS(tcp_server::launch({ep}, cb));

    /* the failed launch must have released the port */
    REQUIRE_NOTHROW(tcp_server::launch({ep}, {}));
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
//     auto l = tcp_server::launch({ep}, cb);
//     l->stop_async();
//     REQUIRE_THROWS(l->wait_until_stopped());
// }
//
// note: still disabled. OnStoppedListening is invoked from asio_io_pool's monitor thread, so a
// throwing callback escapes that thread and terminates the process rather than surfacing here.

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

    auto l = tcp_server::launch({ep}, cb);

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
    l->wait_until_stopped();
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

    auto l = tcp_server::launch({ep}, cb);

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
    l->wait_until_stopped();

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

    auto l = tcp_server::launch({ep}, cb);

    std::jthread th = std::jthread([]() {
        using boost::asio::ip::tcp;

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", std::to_string(PORT));

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);
    });

    REQUIRE_NOTHROW(th.join());
    l->wait_until_stopped();

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

        auto l = tcp_server::launch({{"127.0.0.1", PORT}}, cb);

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

    auto l = tcp_server::launch({ep}, cb);

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

    l->stop_async();
    l->wait_until_stopped();

    // Check client disconnected
    REQUIRE_NOTHROW(th.join());
    REQUIRE(stop_count == 1);
}

TEST_CASE("tcp_server: check destructor works on a server that was only just launched",
          "[sg::net::tcp_server]") {
    using namespace sg::net;

    /* there is no un-started state to test any more: the closest equivalent is destroying a server
     * that has never seen a connection */
    end_point ep("127.0.0.1", PORT);
    { auto l = tcp_server::launch({ep}, {}); }
}

TEST_CASE("tcp_server: check a server can be relaunched on the same endpoint after stop",
          "[sg::net::tcp_server]") {
    using namespace sg::net;

    end_point ep("127.0.0.1", PORT);

    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = [](tcp_server& l, tcp_server::session_id_t id,
                                   const std::byte* data, size_t length) {
        l.session(id)->write(data, length);
    };

    /* "restart" is now drop-and-relaunch: each round must release the port for the next one */
    for (auto round = 0; round < 3; ++round) {
        auto l = tcp_server::launch({ep}, cb);

        tcp_client_sync client;
        client.connect(ep);
        client.write(fmt::format("round{}\n", round));
        REQUIRE(client.read_until("\n") == fmt::format("round{}\n", round));

        l->stop_async();
        l->wait_until_stopped();
    }
}

#if !defined(__APPLE__)
TEST_CASE("tcp_server: you can't listen to same port twice", "[sg::net::tcp_server]") {
    using namespace sg::net;

    // wild-card address
    {
        end_point ep("0.0.0.0", PORT);

        auto server1 = tcp_server::launch({ep}, {});
        REQUIRE_THROWS(tcp_server::launch({ep}, {}));
    }

    // specific address
    {
        end_point ep("127.0.0.1", PORT);

        auto server1 = tcp_server::launch({ep}, {});
        REQUIRE_THROWS(tcp_server::launch({ep}, {}));
    }

    // mix
    {
        end_point ep1("0.0.0.0", PORT);
        end_point ep2("127.0.0.1", PORT);

        auto server1 = tcp_server::launch({ep1}, {});
        REQUIRE_THROWS(tcp_server::launch({ep2}, {}));
    }

    // mix
    {
        end_point ep1("127.0.0.1", PORT);
        end_point ep2("0.0.0.0", PORT);

        auto server1 = tcp_server::launch({ep1}, {});
        REQUIRE_THROWS(tcp_server::launch({ep2}, {}));
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

    auto l = tcp_server::launch({ep}, cb);

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

    auto l = tcp_server::launch({ep}, cb);

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

    /* declaration order matters for teardown: proxy_server's callbacks use client_intermediate, so
     * it must be destroyed first */
    std::unique_ptr<tcp_server> server_top;
    tcp_client client_intermediate;
    std::unique_ptr<tcp_server> proxy_server;
    {
        tcp_server::CallBacks cb;
        cb.OnSessionDataAvailable =
            [](tcp_server& l, tcp_server::session_id_t id, const std::byte* data, size_t length) {
                l.session(id)->write(data, length);
        };
        server_top = tcp_server::launch({main_ep}, cb);
    }

    {
        tcp_session::Callbacks::OnDataAvailable onDataAvailable =
            [&](tcp_session&, const std::byte* data, size_t length) {
                /* this connection is made before proxy_server is launched; previously that meant
                 * an empty session list, now it means there is no server to ask yet */
                if (!proxy_server)
                    return;

                for (const auto& sess : proxy_server->sessions() | std::views::values)
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
        proxy_server = tcp_server::launch({proxy_ep}, cb);
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

    auto l = tcp_server::launch({{"127.0.0.1", PORT}}, cbs);

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
    opts.no_io_threads = no_threads;

    auto l = tcp_server::launch({ep}, cb, opts);

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

    l->stop_async();
    l->wait_until_stopped();
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
        opts.no_io_threads = kThreads;

        auto server = tcp_server::launch({ep}, cb, opts);

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

        server->stop_async();
        server->wait_until_stopped();   // hangs here if stop_async() deadlocks

        stop_clients.store(true, std::memory_order_relaxed);
        for (auto& t : clients)
            t.join();

        teardown_done.store(true, std::memory_order_release);
        watchdog.join();

        REQUIRE_FALSE(corruption.load());
        REQUIRE(server->is_stopped());
        REQUIRE(server->clients_count() == 0);
    }

    // Make sure the run actually exercised the server (e.g. connects succeeded).
    REQUIRE(total_roundtrips.load() > 0);
}

// ---------------------------------------------------------------------------
// Lifecycle tests for the launch-and-own API. These use their own port base so
// they cannot collide with the tests above.
// ---------------------------------------------------------------------------
static port_t PORT2 = 4700;

TEST_CASE("tcp_server: launch returns a listening server", "[sg::net::tcp_server]") {
    std::binary_semaphore started{0};
    std::atomic_int stopped{0};

    tcp_server::CallBacks cb;
    cb.OnStartedListening = [&](tcp_server&) { started.release(); };
    cb.OnStoppedListening = [&](tcp_server&) { stopped++; };

    end_point ep("127.0.0.1", PORT2);

    {
        auto s = tcp_server::launch({ep}, cb);

        /* OnStartedListening has already fired by the time launch() returns */
        REQUIRE(started.try_acquire());
        REQUIRE_FALSE(s->is_stopped());
        REQUIRE(stopped == 0);
    } // destructor stops it

    REQUIRE(stopped == 1);
}

TEST_CASE("tcp_server: echo round-trip", "[sg::net::tcp_server]") {
    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = [](tcp_server& s, tcp_server::session_id_t id, const std::byte* data,
                                   size_t length) { s.session(id)->write(data, length); };

    end_point ep("127.0.0.1", PORT2 + 1);
    auto s = tcp_server::launch({ep}, cb);

    std::vector<std::shared_ptr<tcp_client_sync>> clients;
    for (auto i = 0; i < 20; ++i) {
        auto client = std::make_shared<tcp_client_sync>();
        client->connect(ep);
        client->write(fmt::format("{}\n", i));
        clients.push_back(client);
    }

    for (auto i = 0; i < 20; ++i)
        REQUIRE(clients[i]->read_until("\n") == fmt::format("{}\n", i));
}

TEST_CASE("tcp_server: a failed launch leaves nothing behind",
          "[sg::net::tcp_server]") {
    /* not a local address, so bind() fails */
    end_point bad("8.8.8.8", PORT2 + 2);
    REQUIRE_THROWS(tcp_server::launch({bad}, {}));

    /* empty endpoint list is rejected before anything is built */
    REQUIRE_THROWS_AS(tcp_server::launch({}, {}), std::invalid_argument);

    /* the failed launches must not have leaked a listener: a real one on the same port works */
    end_point good("127.0.0.1", PORT2 + 2);
    REQUIRE_NOTHROW(tcp_server::launch({good}, {}));
}

TEST_CASE("tcp_server: cannot bind the same port twice", "[sg::net::tcp_server]") {
    end_point ep("127.0.0.1", PORT2 + 3);

    tcp_server::options_t opts;
    opts.reuse_address = false;
    opts.exclusive_address_use = true;

    auto first = tcp_server::launch({ep}, {}, opts);
    REQUIRE_THROWS(tcp_server::launch({ep}, {}, opts));

    /* the failed second launch must not have disturbed the first */
    REQUIRE_FALSE(first->is_stopped());
}

TEST_CASE("tcp_server: relaunching replaces the server", "[sg::net::tcp_server]") {
    end_point ep("127.0.0.1", PORT2 + 4);

    /* "restart" is drop-and-relaunch; the port must be free again for the second launch */
    for (auto i = 0; i < 3; ++i) {
        auto s = tcp_server::launch({ep}, {});
        REQUIRE_FALSE(s->is_stopped());

        tcp_client_sync client;
        REQUIRE_NOTHROW(client.connect(ep));
    }
}

TEST_CASE("tcp_server: stop_async() from a callback does not deadlock",
          "[sg::net::tcp_server]") {
    end_point ep("127.0.0.1", PORT2 + 5);

    tcp_server::CallBacks cb;
    /* stopping from inside a session callback is the case that forces the teardown to happen on
     * its own thread rather than the caller's */
    cb.OnSessionCreated = [](tcp_server& s, tcp_server::session_id_t) { s.stop_async(); };

    auto s = tcp_server::launch({ep}, cb);

    tcp_client_sync client;
    client.connect(ep);

    s->wait_until_stopped();
    REQUIRE(s->is_stopped());
}

TEST_CASE("tcp_server: stop_async() from OnStartedListening",
          "[.][sg::net::tcp_server]") {
    end_point ep("127.0.0.1", PORT2 + 9);

    /* OnStartedListening fires before the io context is run, so this asks for a stop while
     * launch() is still building the server.
     *
     * The sleep is deliberate: it hands the race to the teardown thread, which would otherwise
     * almost never win against the very next statement in bind_and_run(). Without it this passes
     * even when the underlying bug is present.
     *
     * Repeated because the sleep narrows the race without removing it. The acceptor-close leak
     * this caught only showed up in about one run in twenty, so a single round would miss it far
     * more often than not. */
    tcp_server::CallBacks cb;
    cb.OnStartedListening = [](tcp_server& s) {
        s.stop_async();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    };

    constexpr int rounds = 30;

    for (int round = 0; round < rounds; ++round) {
        INFO("round " << round);

        auto s = tcp_server::launch({ep}, cb);
        s->wait_until_stopped();
        REQUIRE(s->is_stopped());

        /* The stop must have closed the acceptors, not merely flipped the flag. `s` is still alive
         * here, so nothing but the teardown can have released the endpoint. Re-using the same
         * endpoint every round is what makes that assertion meaningful. */
        REQUIRE_NOTHROW(tcp_server::launch({ep}, {}));
    }
}

TEST_CASE("tcp_server: a throwing OnStartedListening leaves nothing behind",
          "[sg::net::tcp_server]") {
    end_point ep("127.0.0.1", PORT2 + 10);

    tcp_server::CallBacks cb;
    cb.OnStartedListening = [](tcp_server&) { throw std::runtime_error("user callback failed"); };

    REQUIRE_THROWS_AS(tcp_server::launch({ep}, cb), std::runtime_error);

    /* the port must have been released */
    REQUIRE_NOTHROW(tcp_server::launch({ep}, {}));
}

TEST_CASE("tcp_server: OnStoppedListening has run by the time the stop completes",
          "[sg::net::tcp_server]") {
    end_point ep("127.0.0.1", PORT2 + 11);

    std::atomic<bool> reported{false};

    tcp_server::CallBacks cb;
    cb.OnStoppedListening = [&](tcp_server&) { reported.store(true, std::memory_order::release); };

    auto s = tcp_server::launch({ep}, cb);

    s->stop_async();
    s->wait_until_stopped();

    /* The io pool has no work guard, so closing the acceptors lets it drain by itself and report
     * is_running() == false while its monitor thread is still about to fire this callback. That is
     * why teardown() waits for the pool cycle unconditionally rather than skipping it when the
     * pool looks stopped -- otherwise the stop could complete before the server had finished
     * reporting it. */
    REQUIRE(reported.load(std::memory_order::acquire));
}

TEST_CASE("tcp_server: stop_async() is idempotent", "[sg::net::tcp_server]") {
    end_point ep("127.0.0.1", PORT2 + 6);

    auto s = tcp_server::launch({ep}, {});
    s->stop_async();
    s->stop_async();
    s->stop_async();

    s->wait_until_stopped();
    REQUIRE(s->is_stopped());
}

TEST_CASE("tcp_server: clients are dropped when the server goes away",
          "[sg::net::tcp_server]") {
    end_point ep("127.0.0.1", PORT2 + 7);

    std::atomic_int disconnects{0};
    tcp_server::CallBacks cb;
    cb.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) { disconnects++; };

    tcp_client_sync client;
    {
        auto s = tcp_server::launch({ep}, cb);
        client.connect(ep);

        while (s->clients_count() == 0)
            std::this_thread::yield();
        REQUIRE(s->clients_count() == 1);
    }

    REQUIRE(disconnects == 1);
}

TEST_CASE("tcp_server: works across a range of no_threads",
          "[sg::net::tcp_server]") {
    for (size_t threads : {size_t{1}, size_t{2}, size_t{4}}) {
        tcp_server::options_t opts;
        opts.no_io_threads = threads;

        tcp_server::CallBacks cb;
        cb.OnSessionDataAvailable = [](tcp_server& s, tcp_server::session_id_t id, const std::byte* data,
                                       size_t length) { s.session(id)->write(data, length); };

        end_point ep("127.0.0.1", PORT2 + 8);
        auto s = tcp_server::launch({ep}, cb, opts);

        tcp_client_sync client;
        client.connect(ep);
        client.write("ping\n");
        REQUIRE(client.read_until("\n") == "ping\n");
    }
}

// ---------------------------------------------------------------------------
// Session callbacks run on the callback pool, one strand per session.
// ---------------------------------------------------------------------------

TEST_CASE("tcp_server: a blocking session callback does not stall the io threads",
          "[sg::net::tcp_server]") {
    /* One io thread, so that a session callback occupying it would make the server deaf -- which is
     * exactly what used to happen. Several callback threads, so that the second connection is not
     * merely queued behind the first session's blocked callback on a single-threaded pool. */
    tcp_server::options_t opts;
    opts.no_io_threads = 1;
    opts.no_callback_threads = 4;

    std::binary_semaphore parked{0};  // released once a data callback is inside the block
    std::binary_semaphore release{0}; // released to let it out again
    std::binary_semaphore second{0};  // released when a later session is reported

    std::atomic_bool blocked_once{false};
    std::atomic_int created{0};

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server&, tcp_server::session_id_t) {
        if (++created == 2)
            second.release();
    };
    cb.OnSessionDataAvailable = [&](tcp_server&, tcp_server::session_id_t, const std::byte*,
                                    size_t) {
        /* only ever block on the first notification: a second one would wait on a semaphore that is
         * never released again and hang teardown */
        if (!blocked_once.exchange(true)) {
            parked.release();
            release.acquire();
        }
    };

    end_point ep("127.0.0.1", PORT2 + 12);
    auto s = tcp_server::launch({ep}, cb, opts);

    /* Declared AFTER the server, so it is destroyed BEFORE it: the server's destructor waits for
     * every session callback to finish, so the parked one has to be let out first or teardown
     * deadlocks. Being a destructor, it also covers a failing REQUIRE below, which aborts by
     * throwing. */
    struct unpark {
        std::binary_semaphore& sem;
        ~unpark() { sem.release(); }
    } unparker{release};

    tcp_client_sync first;
    first.connect(ep);
    first.write("x\n");

    REQUIRE(parked.try_acquire_for(std::chrono::seconds(5)));

    /* the single io thread must still be free to accept and report a new connection */
    tcp_client_sync later;
    later.connect(ep);
    REQUIRE(second.try_acquire_for(std::chrono::seconds(5)));
}

TEST_CASE("tcp_server: a session's callbacks arrive in order", "[sg::net::tcp_server]") {
    tcp_server::options_t opts;
    opts.no_io_threads = 4;
    opts.no_callback_threads = 4;

    constexpr int messages = 200;

    std::mutex mutex;
    std::vector<std::string> events;
    std::binary_semaphore done{0};

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server&, tcp_server::session_id_t) {
        std::lock_guard lock(mutex);
        events.emplace_back("created");
    };
    cb.OnSessionDataAvailable = [&](tcp_server&, tcp_server::session_id_t, const std::byte* data,
                                    size_t length) {
        std::lock_guard lock(mutex);
        events.emplace_back("data:" +
                            std::string(reinterpret_cast<const char*>(data), length));
    };
    cb.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        {
            std::lock_guard lock(mutex);
            events.emplace_back("disconnected");
        }
        done.release();
    };

    end_point ep("127.0.0.1", PORT2 + 13);
    auto s = tcp_server::launch({ep}, cb, opts);

    std::string expected;
    {
        tcp_client_sync client;
        client.connect(ep);
        for (int i = 0; i < messages; ++i) {
            auto msg = fmt::format("{},", i);
            client.write(msg);
            expected += msg;
        }
    } // client goes away, so the session disconnects

    REQUIRE(done.try_acquire_for(std::chrono::seconds(10)));

    std::lock_guard lock(mutex);
    REQUIRE(events.size() >= 3);

    /* OnSessionCreated is posted before the reader can produce anything, and OnDisconnected after
     * the reader has finished, so on one strand they must bracket the data */
    REQUIRE(events.front() == "created");
    REQUIRE(events.back() == "disconnected");

    /* and the payload must arrive whole and in order, however the reads happened to split it */
    std::string payload;
    for (size_t i = 1; i + 1 < events.size(); ++i) {
        REQUIRE(events[i].starts_with("data:"));
        payload += events[i].substr(5);
    }
    REQUIRE(payload == expected);
}

TEST_CASE("tcp_server: a peer that outruns its consumer is throttled, not dropped",
          "[sg::net::tcp_server]") {
    /* Comfortably above the 64 KiB floor the server puts under the pause threshold, so that the
     * threshold is the size the socket reports and this test can size its flood off that. Asking for
     * less would land under the floor, where the reported size is no longer the threshold. */
    tcp_server::options_t opts;
    opts.session_options.recv_buffer_size = 256 * 1024;

    std::binary_semaphore created{0};
    std::binary_semaphore done{0};
    size_t threshold = 0;
    std::atomic<size_t> received{0};
    std::atomic_int pauses_seen{0};

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server& srv, tcp_server::session_id_t id) {
        threshold = static_cast<size_t>(
            sg::net::native::get_recv_buffer_size(srv.session(id)->native_handle()));
        created.release();
    };
    cb.OnSessionDataAvailable = [&](tcp_server& srv, tcp_server::session_id_t id, const std::byte*,
                                    size_t length) {
        /* a consumer slower than the network, so the backlog can only grow */
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        received += length;

        /* Observed from inside the callback rather than polled from the test thread: our own bytes
         * are only released once we return, so if the backlog crossed the mark the session is still
         * paused as we look. */
        if (srv.session(id)->is_reading_paused())
            ++pauses_seen;
    };
    cb.OnDisconnected = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr) {
        done.release();
    };

    end_point ep("127.0.0.1", PORT2 + 15);
    auto s = tcp_server::launch({ep}, cb, opts);

    /* tcp_client rather than tcp_client_sync: writes are queued on the session, so flooding cannot
     * block this thread once the peer's window shuts. */
    tcp_client client;
    client.connect(ep, nullptr, nullptr);

    /* Size the flood off what the socket actually reports rather than off recv_buffer_size, because
     * that is what the pause threshold is: platforms are free to round the request (Linux doubles it,
     * and every platform clamps to its own maximum), and a fixed flood smaller than the result would
     * leave the backlog unable to reach the mark at all -- nothing would ever be throttled and the
     * test would prove nothing. */
    REQUIRE(created.try_acquire_for(std::chrono::seconds(10)));

    /* the semaphore orders the write of `threshold` against this read */
    REQUIRE(threshold > 0);

    std::string chunk(16 * 1024, 'x');
    const size_t chunks = 2 * (threshold / chunk.size() + 1);
    for (size_t i = 0; i < chunks; ++i)
        client.session().write(chunk);

    /* Flushes the queued writes and then closes, which is what ends the session. A throttled peer
     * only delays delivery, so by then the server must have every byte -- that, rather than any
     * exception, is what says it was slowed instead of shed. */
    client.disconnect();

    REQUIRE(done.try_acquire_for(std::chrono::seconds(30)));

    /* the semaphore orders the writes below against these reads */
    REQUIRE(received == chunks * chunk.size());
    REQUIRE(pauses_seen > 0);
}

TEST_CASE("tcp_server: a session paused for back-pressure still shuts down",
          "[sg::net::tcp_server]") {
    /* A paused reader is suspended on an event no socket operation touches, so unless closing the
     * session cancels it the reader never wakes: teardown would finish but the session would live on
     * inside the io context, and its coroutine frame with it. */
    tcp_server::options_t opts;
    opts.session_options.recv_buffer_size = 256 * 1024;

    std::binary_semaphore created{0};
    std::binary_semaphore gate{0};
    std::atomic_bool first{true};
    std::atomic_bool gate_timed_out{false};
    tcp_server::session_id_t session_id = 0;

    tcp_server::CallBacks cb;
    cb.OnSessionCreated = [&](tcp_server&, tcp_server::session_id_t id) {
        session_id = id;
        created.release();
    };
    cb.OnSessionDataAvailable = [&](tcp_server&, tcp_server::session_id_t, const std::byte*,
                                    size_t) {
        /* Holds the whole backlog: nothing is released while we are here, so the pause below is not a
         * race -- once the flood exceeds the mark the session stays paused until we return.
         *
         * The outcome is recorded rather than asserted, because a Catch2 assertion off the main
         * thread is itself a data race. */
        if (first.exchange(false) && !gate.try_acquire_for(std::chrono::seconds(30)))
            gate_timed_out = true;
    };

    end_point ep("127.0.0.1", PORT2 + 17);
    auto s = tcp_server::launch({ep}, cb, opts);

    tcp_client client;
    client.connect(ep, nullptr, nullptr);

    REQUIRE(created.try_acquire_for(std::chrono::seconds(10)));

    const auto threshold = static_cast<size_t>(
        sg::net::native::get_recv_buffer_size(s->session(session_id)->native_handle()));
    REQUIRE(threshold > 0);

    std::string chunk(16 * 1024, 'x');
    for (size_t i = 0, chunks = 4 * (threshold / chunk.size() + 1); i < chunks; ++i)
        client.session().write(chunk);

    /* the pause is the state under test, so wait for it rather than assume it */
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!s->session(session_id)->is_reading_paused() &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    REQUIRE(s->session(session_id)->is_reading_paused());

    /* Requested while paused, and only then is the consumer let go, so the close has to be what
     * wakes the reader. */
    s->stop_async();
    gate.release();

    auto stopped = std::async(std::launch::async, [&] { s->wait_until_stopped(); });
    REQUIRE(stopped.wait_for(std::chrono::seconds(30)) == std::future_status::ready);
    REQUIRE_FALSE(gate_timed_out);
}

TEST_CASE("tcp_server: a consumer that keeps up is never throttled, however small the buffer",
          "[sg::net::tcp_server]") {
    /* The smallest buffer the OS will give us. The mark's 64 KiB floor is what keeps strict
     * request/response off the back-pressure path entirely, however low this is -- without it a
     * message spanning two reads could pause a consumer that is perfectly up to date. */
    tcp_server::options_t opts;
    opts.session_options.recv_buffer_size = 1;

    std::atomic_int pauses_seen{0};

    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = [&](tcp_server& srv, tcp_server::session_id_t id,
                                    const std::byte* data, size_t length) {
        if (srv.session(id)->is_reading_paused())
            ++pauses_seen;
        srv.session(id)->write(data, length);
    };

    end_point ep("127.0.0.1", PORT2 + 16);
    auto s = tcp_server::launch({ep}, cb, opts);

    {
        tcp_client_sync client;
        client.connect(ep);
        for (int i = 0; i < 20; ++i) {
            auto msg = fmt::format("ping{}\n", i);
            client.write(msg);
            REQUIRE(client.read_until("\n") == msg);
        }
    }

    REQUIRE(pauses_seen == 0);
}

TEST_CASE("tcp_server: dont_read sessions still report data", "[sg::net::tcp_server]") {
    /* dont_read cannot be moved off the io threads -- the user reads the native handle, which may
     * only be touched on the session's io strand -- so it keeps the old synchronous contract:
     * the callback is told there is data, and is handed no buffer. */
    tcp_server::options_t opts;
    opts.session_options.dont_read = true;

    std::binary_semaphore notified{0};
    std::atomic_bool fired{false};
    std::atomic_bool handed_no_buffer{false};

    tcp_server::CallBacks cb;
    cb.OnSessionDataAvailable = [&](tcp_server& srv, tcp_server::session_id_t id,
                                    const std::byte* data, size_t length) {
        if (fired.exchange(true))
            return;

        handed_no_buffer = (data == nullptr && length == 0);

        /* we are deliberately not draining the socket, so the readiness notification would
         * otherwise repeat forever; drop the session to end it */
        srv.disconnect(id);
        notified.release();
    };

    end_point ep("127.0.0.1", PORT2 + 14);
    auto s = tcp_server::launch({ep}, cb, opts);

    tcp_client_sync client;
    client.connect(ep);
    client.write("hello");

    REQUIRE(notified.try_acquire_for(std::chrono::seconds(5)));
    REQUIRE(handed_no_buffer);
}