#include <catch2/catch_test_macros.hpp>

#include <sg/tcp_server.h>
#include <sg/jthread.h>
#include <sg/random.h>

#include <boost/asio.hpp>

#include <atomic>
#include <semaphore>
#include <string>

static sg::net::port_t PORT = 4444; // 55555 can't be used on macOS!

TEST_CASE("sg::net::tcp_server: check bad endpoint throws exception during start()", "[sg::net::tcp_server]") {
    sg::net::end_point ep;
    ep.ip = PORT;
    ep.ip = "8.8.8.8";

    sg::net::tcp_server l;
    REQUIRE_THROWS(l.start({ep}, nullptr, nullptr,nullptr, nullptr, nullptr));
}

TEST_CASE("sg::net::tcp_server: check start/stop callback", "[sg::net::tcp_server]") {
    std::atomic_int stop_count{0};
    std::binary_semaphore start_sem{0};

    using namespace sg::net;

    tcp_server::started_listening_cb_t onStart = [&](tcp_server&) { start_sem.release(); };
    tcp_server::stopped_listening_cb_t onStop = [&](tcp_server&) { stop_count++; };

    tcp_server::session_disconnected_cb_t onDisconn =
        [&](tcp_server&, tcp_server::session_id_t, std::optional<std::exception>) { stop_count++; };

    sg::net::end_point ep("127.0.0.1", PORT);

    tcp_server l;
    l.start({ep}, onStart, onStop, nullptr, nullptr,nullptr);

    start_sem.acquire();
    REQUIRE(stop_count == 0);

    l.stop_async();
    l.future_get_once();
    REQUIRE(stop_count == 1);
}

struct tcp_server_test0 {
    std::atomic_int stop_count{0};
    std::binary_semaphore start_sem{0};

    sg::net::tcp_server l;
    void start() {
        sg::net::end_point ep("127.0.0.1", PORT);
        auto onstart = std::bind(&tcp_server_test0::on_start, this, std::placeholders::_1);
        auto onstop = std::bind(&tcp_server_test0::on_stop, this, std::placeholders::_1);
        l.start({ep}, onstart, onstop, nullptr, nullptr,nullptr);

    }
    void on_start(sg::net::tcp_server&) {
       start_sem.release();
    }
    void on_stop(sg::net::tcp_server&) {
        stop_count++;
    }
    void on_disconn(sg::net::tcp_server&, sg::net::tcp_server::session_id_t, std::optional<std::exception>) {
        stop_count++;
    }
};

TEST_CASE("sg::net::tcp_server: check start/stop callback as class member", "[sg::net::tcp_server]") {
    tcp_server_test0 t;
    t.start();
    t.start_sem.acquire();
    REQUIRE(t.stop_count == 0);

    t.l.stop_async();
    t.l.future_get_once();
    REQUIRE(t.stop_count == 1);
}


TEST_CASE("sg::net::tcp_server: check read/write with many simultanious clients", "[sg::net::tcp_server]") {
    using namespace sg::net;

    int count =100;
    std::atomic_int counterNew{0};
    std::atomic_int counterClosed{0};

    tcp_server::session_data_available_cb_t on_data =
        [](tcp_server&l,tcp_server::session_id_t id, const std::byte* dat, size_t size) {
            auto w =sg::make_shared_c_buffer<std::byte>(size);
            std::memcpy(w.get(), dat, size);
            l.write(id, w);
        };

    tcp_server::session_created_cb_t onNew = [&counterNew](tcp_server&, tcp_server::session_id_t) {
        counterNew++;
    };

    tcp_server::session_disconnected_cb_t onClose = [&counterClosed, count](tcp_server& l, tcp_server::session_id_t, std::optional<std::exception>) {
        counterClosed++;
        if (counterClosed == count)
            l.stop_async();
    };


    sg::net::end_point ep("0.0.0.0", PORT);

    tcp_server l;
    l.start({ep}, nullptr, nullptr, onNew, on_data, onClose);

    auto func = []() {
        using boost::asio::ip::tcp;

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", std::to_string(PORT));

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        boost::system::error_code error;
        auto buf_write = sg::random::genrate<char>(20);
        std::vector<char> buf_read(20);

        socket.write_some(boost::asio::buffer(buf_write), error);
        socket.read_some(boost::asio::buffer(buf_read), error);

        if (buf_write != buf_read) throw std::runtime_error("error");

        if (error) throw boost::system::system_error(error);

        socket.shutdown(tcp::socket::shutdown_type::shutdown_both);
        socket.close();
    };

    std::vector<std::thread> threads;
    for (int i=0; i <count; i++)
        threads.emplace_back(std::thread(func));

    for (auto& th: threads)
        REQUIRE_NOTHROW(th.join());

    l.future_get_once();

    REQUIRE(counterNew.load() ==count);
    REQUIRE(counterClosed.load()==count);
}

TEST_CASE("sg::net::tcp_server: check can disconnect client", "[sg::net::tcp_server]") {
    using namespace sg::net;

    tcp_server::session_data_available_cb_t on_data =
        [](tcp_server&l,tcp_server::session_id_t id, const std::byte*, size_t) {
            l.disconnect(id);
        };

    tcp_server::session_disconnected_cb_t on_disconn = [](tcp_server& l, sg::net::tcp_server::session_id_t, std::optional<std::exception>){
        l.stop_async();
    };

    sg::net::end_point ep("0.0.0.0", PORT);

    tcp_server l;
    l.start({ep}, nullptr, nullptr, nullptr, on_data, on_disconn);

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


TEST_CASE("sg::net::tcp_server: check what happens if client disconnects", "[sg::net::tcp_server]") {
    using namespace sg::net;

    std::atomic_bool has_exception{false};

    tcp_server::session_disconnected_cb_t on_disconn = [&](tcp_server& l, sg::net::tcp_server::session_id_t, std::optional<std::exception> ex){
        if (ex)
            has_exception=true;
        l.stop_async();
    };

    sg::net::end_point ep("0.0.0.0", PORT);

    tcp_server l;
    l.start({ep}, nullptr, nullptr, nullptr, nullptr, on_disconn);

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

TEST_CASE("sg::net::tcp_server: check what happens if there is an exception in started_listening_cb_t cb", "[sg::net::tcp_server]") {
    using namespace sg::net;

    tcp_server::started_listening_cb_t onListening =
        [](tcp_server&) {
            throw std::runtime_error("bad error!");
        };

    sg::net::end_point ep("0.0.0.0", PORT);
    tcp_server l;

    REQUIRE_THROWS(l.start({ep}, onListening, nullptr, nullptr, nullptr, nullptr));
}

TEST_CASE("sg::net::tcp_server: check what happens if there is an exception in stopped_listening_cb_t cb", "[sg::net::tcp_server]") {
    using namespace sg::net;

    tcp_server::stopped_listening_cb_t onStop =
        [](tcp_server&) {
            throw std::runtime_error("bad error!");
        };

    sg::net::end_point ep("0.0.0.0", PORT);
    auto l = tcp_server();
    l.start({ep}, nullptr, onStop, nullptr, nullptr, nullptr);
    l.stop_async();
    REQUIRE_THROWS(l.future_get_once());
}


TEST_CASE("sg::net::tcp_server: check session(...)", "[sg::net::tcp_server]") {
    using namespace sg::net;

    tcp_server::session_data_available_cb_t on_data =
        [](tcp_server&l,tcp_server::session_id_t id, const std::byte* dat, size_t size) {
            auto w =sg::make_shared_c_buffer<std::byte>(size);
            std::memcpy(w.get(), dat, size);
            l.session(id)->write(w);
        };

    tcp_server::session_disconnected_cb_t on_disconn = [](tcp_server& l, sg::net::tcp_server::session_id_t, std::optional<std::exception>){
        l.stop_async();
    };

    sg::net::end_point ep("0.0.0.0", PORT);

    tcp_server l;
    l.start({ep}, nullptr, nullptr, nullptr, on_data, on_disconn);

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

TEST_CASE("sg::net::tcp_server: check local/remote_endpoint(...)", "[sg::net::tcp_server]") {
    using namespace sg::net;

    tcp_server::session_data_available_cb_t on_data =
        [](tcp_server&l,tcp_server::session_id_t id, const std::byte* dat, size_t size) {
            auto w =sg::make_shared_c_buffer<std::byte>(size);
            std::memcpy(w.get(), dat, size);

            auto local = l.session(id)->local_endpoint();
            auto remote = l.session(id)->remote_endpoint();
            REQUIRE(local.ip == remote.ip);
            REQUIRE(local.port == PORT);

            l.session(id)->write(w);
        };

    tcp_server::session_disconnected_cb_t on_disconn = [](tcp_server& l, sg::net::tcp_server::session_id_t, std::optional<std::exception>){
        l.stop_async();
    };

    sg::net::end_point ep("0.0.0.0", PORT);

    tcp_server l;
    l.start({ep}, nullptr, nullptr, nullptr, on_data, on_disconn);

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

TEST_CASE("sg::net::tcp_server: check reaction to client immediate disconnection", "[sg::net::tcp_server]") {
    using namespace sg::net;

    std::atomic_bool boolCon {false};
    std::atomic_bool boolDis {false};

    tcp_server::session_created_cb_t on_conn = [&](tcp_server&, sg::net::tcp_server::session_id_t){
        boolCon = true;
    };
    tcp_server::session_disconnected_cb_t on_disconn = [&](tcp_server& l, sg::net::tcp_server::session_id_t, std::optional<std::exception>){
        boolDis = true;
        l.stop_async();
    };

    sg::net::end_point ep("0.0.0.0", PORT);

    tcp_server l;
    l.start({ep}, nullptr, nullptr, on_conn, nullptr, on_disconn);

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

    REQUIRE(boolCon==true);
    REQUIRE(boolDis==true);
}

TEST_CASE("sg::net::tcp_server: check dropping tcp_server drops allconnections", "[sg::net::tcp_server]") {
    using namespace sg::net;

    std::binary_semaphore sem{0};
    std::atomic_int stop_count{0};

    tcp_server::session_created_cb_t onConn = [&](tcp_server&, tcp_server::session_id_t) { sem.release(); };
    tcp_server::stopped_listening_cb_t onStop = [&](tcp_server&) { stop_count++; };

    sg::net::end_point ep("0.0.0.0", PORT);
    std::jthread th;

    {
        tcp_server l;
        l.start({ep}, nullptr, onStop, onConn, nullptr, nullptr);

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
    REQUIRE(stop_count ==1);
}

TEST_CASE("sg::net::tcp_server: check stop_async() drops all connections", "[sg::net::tcp_server]") {
    using namespace sg::net;

    std::binary_semaphore sem{0};
    std::atomic_int stop_count{0};

    tcp_server::session_created_cb_t onConn = [&](tcp_server&, tcp_server::session_id_t) {
        sem.release(); }
    ;
    tcp_server::stopped_listening_cb_t onStop = [&](tcp_server&) { stop_count++; };

    sg::net::end_point ep("0.0.0.0", PORT);
    std::jthread th;

    tcp_server l;
    l.start({ep}, nullptr, onStop, onConn, nullptr, nullptr);

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
    REQUIRE(stop_count ==1);
}

TEST_CASE("sg::net::tcp_server: check destructor works if start(...) not started", "[sg::net::tcp_server]") {
    using namespace sg::net;

    {
        tcp_server l;
    }
}
