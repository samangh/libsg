#include "sg/tcp_context.h"

#include <catch2/catch_test_macros.hpp>
#include <sg/tcp_context.h>

using namespace sg::net;

TEST_CASE("sg::net::asio_context_pool: check unused context", "[sg::net::tcp_context]") {
    {
        auto context = tcp_context::create(1);
        auto context2 = tcp_context::create(4);
    }

    {
        auto context = tcp_context::create(1);
        auto context2 = tcp_context::create(4);
        context->run();
        context2->run();
    }
}

TEST_CASE("sg::net::asio_context_pool check stop() calls callback", "[sg::net::tcp_context]") {
        std::atomic<bool> called{false};

        tcp_context::stopped_cb_t onStop = [&called](tcp_context&) {
            called.store(true);
            called.notify_all();
        };
        auto context = tcp_context::create(2, onStop);

        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard =
            boost::asio::make_work_guard(context->context());
        context->run();
        context->stop_async();
        called.wait(false);
}

TEST_CASE("sg::net::asio_context_pool check destructor calls callback", "[sg::net::tcp_context]") {
    std::atomic<bool> called{false};

    {
        tcp_context::stopped_cb_t onStop = [&called](tcp_context&) {
            called.store(true);
            called.notify_all();
        };
        auto context = tcp_context::create(2, onStop);
        context->run();

    }

    called.wait(false);
}

TEST_CASE("sg::net::asio_context_pool callback called once", "[sg::net::tcp_context]") {
    std::atomic<int> count{0};

    {
        tcp_context::stopped_cb_t onStop = [&count](tcp_context&) {
            ++count;
            count.notify_all();
        };
        auto context = tcp_context::create(3, onStop);
        context->run();
    }

    REQUIRE(count == 1);
}


TEST_CASE("sg::net::asio_context_pool check stop_async() can be called from a context thread", "[sg::net::tcp_context]") {
    std::atomic<bool> called{false};

    tcp_context::stopped_cb_t onStop = [&called](tcp_context&) {
        called.store(true);
        called.notify_all();
    };
    auto context = tcp_context::create(2, onStop);

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard =
        boost::asio::make_work_guard(context->context());
    context->run();
    boost::asio::post(context->context(), [&context]() { context->stop_async(); });

    called.wait(false);
}

TEST_CASE("sg::net::asio_context_pool check tcp_context() will stop when guard is finished", "[sg::net::tcp_context]") {
    std::atomic<bool> called{false};

    tcp_context::stopped_cb_t onStop = [&called](tcp_context&) {
        called.store(true);
        called.notify_all();
    };
    auto context = tcp_context::create(2, onStop);
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard =
    boost::asio::make_work_guard(context->context());

    context->run();

    boost::asio::post(context->context(), [&guard]() { guard.reset(); });

    called.wait(false);
}

TEST_CASE("sg::net::asio_context_pool running() a running context throws error", "[sg::net::tcp_context]") {
    std::atomic<bool> called{false};

    tcp_context::stopped_cb_t onStop = [&called](tcp_context&) {
        called.store(true);
        called.notify_all();
    };
    auto context = tcp_context::create(2, onStop);
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard =
    boost::asio::make_work_guard(context->context());

    context->run();
    REQUIRE_THROWS(context->run());
}

TEST_CASE("sg::net::asio_context_pool check resetting a non-guarded context throws error", "[sg::net::tcp_context]") {
    auto context = tcp_context::create(2, nullptr);

    /* make sure the context is runnng */
    std::binary_semaphore stop{false};
    context->context().post([&stop]() {
        stop.acquire();
    });

    context->run();
    REQUIRE_THROWS(context->reset_guard());
    stop.release();
}

TEST_CASE("sg::net::asio_context_pool check resetting a non-running context is OK", "[sg::net::tcp_context]") {
    auto context = tcp_context::create(2, nullptr);

    REQUIRE_NOTHROW(context->reset_guard());
}

TEST_CASE("sg::net::asio_context_pool you can stop without resetting", "[sg::net::tcp_context]") {
    // Destructor
    {
        auto context = tcp_context::create(2, nullptr);
        context->run(true);
    }

    // Direct stop
    {
        auto context = tcp_context::create(2, nullptr);
        context->run(true);
        context->stop_async();
    }
}

TEST_CASE("sg::net::asio_context_pool check guard_reset()", "[sg::net::tcp_context]") {
    for (int i=0; i<100; ++i) {
        auto context = tcp_context::create(2, nullptr);

        context->run(true);
        REQUIRE(context->is_running());

        context->reset_guard();
        context->future_get_once();
    }
}

TEST_CASE("sg::net::asio_context_pool exception on get_future_once()", "[sg::net::tcp_context]") {
    auto context = tcp_context::create(3, nullptr);

    context->context().post([]() {
        throw std::invalid_argument("tes");
    });

    context->run();
    context->wait_for_stop();
    REQUIRE_THROWS(context->future_get_once());
}