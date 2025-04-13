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

    tcp_server::new_session_cb_t onNew = [&counterNew](tcp_server&, tcp_server::session_id_t) {
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

        //std::array<char, 20> buf_read;

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
