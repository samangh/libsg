#include <sg/tcp_listener.h>
#include <sg/jthread.h>
#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <boost/asio.hpp>

#include <atomic>
#include <semaphore>
#include <string>

#define PORT 4444 // 55555 can't be used on macOS!

TEST_CASE("sg::tcp_listener: check start/stop callback", "[sg::tcp_listener]") {
    std::atomic_int stop_count{0};
    std::binary_semaphore start_sem{0};
    std::binary_semaphore stop_sem{0};

    sg::tcp_listener::on_started_fn started_cb = [&](sg::tcp_listener&) { start_sem.release(); };
    sg::tcp_listener::on_stopped_fn stopped_cb = [&](sg::tcp_listener&) { stop_count++; };

    sg::tcp_listener l;

    l.start("127.0.0.1", PORT, nullptr, nullptr, nullptr, started_cb, stopped_cb, nullptr);
    start_sem.acquire();
    REQUIRE(stop_count == 0);

    l.stop();
    REQUIRE(stop_count == 1);
}

TEST_CASE("sg::tcp_listener: check start exception", "[sg::tcp_listener]") {
    sg::tcp_listener l;
    REQUIRE_THROWS(l.start("8.8.8.8", PORT, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
}

TEST_CASE("sg::tcp_listener: check read/write", "[sg::tcp_listener]") {
    sg::tcp_listener l;

    sg::tcp_listener::on_data_available_fn on_data =
        [](sg::tcp_listener& l, sg::tcp_listener::client_id id, sg::tcp_listener::buffer&& buffer) {
            l.write(id, std::move(buffer));
        };
    sg::tcp_listener::on_client_disconnected_fn on_disconn =
        [](sg::tcp_listener& l, sg::tcp_listener::client_id) { l.stop_async(); };

    l.start("127.0.0.1", PORT, nullptr, nullptr, on_disconn, nullptr, nullptr, on_data);

    std::jthread th = std::jthread([]() {
        using boost::asio::ip::tcp;

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", std::to_string(PORT));

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        boost::system::error_code error;
        std::array<char, 5> buf_write = {'H', 'E', 'L', 'L', 'O'};
        std::array<char, 5> buf_read;

        socket.write_some(boost::asio::buffer(buf_write), error);
        socket.read_some(boost::asio::buffer(buf_read), error);

        if (buf_write != buf_read) throw std::runtime_error("error");

        if (error) throw boost::system::system_error(error);

        socket.shutdown(tcp::socket::shutdown_type::shutdown_both);
        socket.close();
    });

    REQUIRE_NOTHROW(th.join());
}

TEST_CASE("sg::tcp_listener: check can disconnect client", "[sg::tcp_listener]") {
    sg::tcp_listener l;

    sg::tcp_listener::on_data_available_fn on_data =
        [](sg::tcp_listener& l, sg::tcp_listener::client_id id, sg::tcp_listener::buffer&&) {
            l.disconnect_async(id);
        };
    sg::tcp_listener::on_client_disconnected_fn on_disconn =
        [](sg::tcp_listener& l, sg::tcp_listener::client_id) { l.stop_async(); };

    l.start("127.0.0.1", PORT, nullptr, nullptr, on_disconn, nullptr, nullptr, on_data);

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

TEST_CASE("sg::tcp_listener: check templated write", "[sg::tcp_listener]") {
    sg::tcp_listener l;

    sg::tcp_listener::on_client_connected_fn on_conn =
        [](sg::tcp_listener& l, sg::tcp_listener::client_id id) {
            l.write(id, "HELLO");
        };

    sg::tcp_listener::on_client_disconnected_fn on_disconn =
        [](sg::tcp_listener& l, sg::tcp_listener::client_id) { l.stop_async(); };

    l.start("127.0.0.1", PORT, nullptr, on_conn, on_disconn, nullptr, nullptr, nullptr);

    std::jthread th = std::jthread([]() {
        using boost::asio::ip::tcp;

        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve("127.0.0.1", std::to_string(PORT));

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        boost::system::error_code error;
        std::array<char, 6> buf_check = {'H', 'E', 'L', 'L', 'O', '\0'};
        std::array<char, 6> buf_read;

        for (;;)
        {
            auto len = socket.read_some(boost::asio::buffer(buf_read), error);
            if (len!=0)
                break;
        }

        if (buf_check != buf_read) throw std::runtime_error("error");

        if (error) throw boost::system::system_error(error);

        socket.shutdown(tcp::socket::shutdown_type::shutdown_both);
        socket.close();
    });

    REQUIRE_NOTHROW(th.join());
}


