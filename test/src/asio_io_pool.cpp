#include "sg/asio_io_pool.h"

#include <boost/asio.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace sg::net;

TEST_CASE("sg::net::asio_context_pool: check unused context", "[sg::net::asio_io_pool]") {
    {
        auto context = asio_io_pool::create(1);
        auto context2 = asio_io_pool::create(4);
    }

    {
        auto context = asio_io_pool::create(1);
        auto context2 = asio_io_pool::create(4);
        context->run();
        context2->run();
    }
}

TEST_CASE("sg::net::asio_context_pool check stop() calls callback", "[sg::net::asio_io_pool]") {
        std::atomic<bool> called{false};

        asio_io_pool::stopped_cb_t onStop = [&called](asio_io_pool&) {
            called.store(true);
            called.notify_all();
        };
        auto context = asio_io_pool::create(2, onStop);

        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard =
            boost::asio::make_work_guard(context->context());
        context->run();
        context->stop_async();
        called.wait(false);
}

TEST_CASE("sg::net::asio_context_pool check destructor calls callback", "[sg::net::asio_io_pool]") {
    std::atomic<bool> called{false};

    {
        asio_io_pool::stopped_cb_t onStop = [&called](asio_io_pool&) {
            called.store(true);
            called.notify_all();
        };
        auto context = asio_io_pool::create(2, onStop);
        context->run();

    }

    called.wait(false);
}

TEST_CASE("sg::net::asio_context_pool callback called once", "[sg::net::asio_io_pool]") {
    std::atomic<int> count{0};

    {
        asio_io_pool::stopped_cb_t onStop = [&count](asio_io_pool&) {
            ++count;
            count.notify_all();
        };
        auto context = asio_io_pool::create(3, onStop);
        context->run();
    }

    REQUIRE(count == 1);
}


TEST_CASE("sg::net::asio_context_pool check stop_async() can be called from a context thread", "[sg::net::asio_io_pool]") {
    std::atomic<bool> called{false};

    asio_io_pool::stopped_cb_t onStop = [&called](asio_io_pool&) {
        called.store(true);
        called.notify_all();
    };
    auto context = asio_io_pool::create(2, onStop);

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard =
        boost::asio::make_work_guard(context->context());
    context->run();
    boost::asio::post(context->context(), [&context]() { context->stop_async(); });

    called.wait(false);
}

TEST_CASE("sg::net::asio_context_pool check pool is stopped after guard reset", "[sg::net::asio_io_pool]") {
    std::atomic<bool> called{false};

    asio_io_pool::stopped_cb_t onStop = [&called](asio_io_pool&) {
        called.store(true);
        called.notify_all();
    };
    auto context = asio_io_pool::create(2, onStop);
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard =
    boost::asio::make_work_guard(context->context());

    context->run();

    boost::asio::post(context->context(), [&guard]() { guard.reset(); });

    called.wait(false);
}

TEST_CASE("sg::net::asio_context_pool running() a running context throws error", "[sg::net::asio_io_pool]") {
    std::atomic<bool> called{false};

    asio_io_pool::stopped_cb_t onStop = [&called](asio_io_pool&) {
        called.store(true);
        called.notify_all();
    };
    auto context = asio_io_pool::create(2, onStop);
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard =
    boost::asio::make_work_guard(context->context());

    context->run();
    REQUIRE_THROWS(context->run());
}

TEST_CASE("sg::net::asio_context_pool check resetting a non-guarded context throws error", "[sg::net::asio_io_pool]") {
    auto context = asio_io_pool::create(2, nullptr);

    /* make sure the context is running */
    std::binary_semaphore stop{false};
    context->context().post([&stop]() {
        stop.acquire();
    });

    context->run();
    REQUIRE_THROWS(context->reset_guard());
    stop.release();
}

TEST_CASE("sg::net::asio_context_pool check resetting a non-running context is OK", "[sg::net::asio_io_pool]") {
    auto context = asio_io_pool::create(2, nullptr);

    REQUIRE_NOTHROW(context->reset_guard());
}

TEST_CASE("sg::net::asio_context_pool you can stop without resetting", "[sg::net::asio_io_pool]") {
    // Destructor
    {
        auto context = asio_io_pool::create(2, nullptr);
        context->run(true);
    }

    // Direct stop
    {
        auto context = asio_io_pool::create(2, nullptr);
        context->run(true);
        context->stop_async();
    }
}

TEST_CASE("sg::net::asio_context_pool check guard_reset()", "[sg::net::asio_io_pool]") {
    for (int i=0; i<100; ++i) {
        auto context = asio_io_pool::create(2, nullptr);

        context->run(true);
        REQUIRE(context->is_running());

        context->reset_guard();
        context->future_get_once();
    }
}

TEST_CASE("sg::net::asio_context_pool exception on get_future_once()", "[sg::net::asio_io_pool]") {
    auto context = asio_io_pool::create(3, nullptr);

    context->context().post([]() {
        throw std::invalid_argument("tes");
    });

    context->run();
    context->wait_for_stop();
    REQUIRE_THROWS(context->future_get_once());
}