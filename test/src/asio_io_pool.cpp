#include <catch2/catch_test_macros.hpp>

#include "sg/asio_io_pool.h"

#include <boost/asio/post.hpp>

using namespace sg::net;

TEST_CASE("asio_context_pool: check unused context", "[sg::net::asio_io_pool]") {
    {
        auto context = asio_io_pool::create(1, false, nullptr);
        auto context2 = asio_io_pool::create(4, false, nullptr);
    }

    {
        auto context = asio_io_pool::create(1, false, nullptr);
        auto context2 = asio_io_pool::create(4, false, nullptr);
        context->run();
        context2->run();
    }
}

TEST_CASE("asio_context_pool: check stop() calls callback", "[sg::net::asio_io_pool]") {
        std::atomic<bool> called{false};

        asio_io_pool::stopped_cb_t onStop = [&called](asio_io_pool&) {
            called.store(true);
            called.notify_all();
        };
        auto context = asio_io_pool::create(2, true, onStop);

        context->run();
        context->stop_async();
        called.wait(false);
}

TEST_CASE("asio_context_pool: check destructor calls callback", "[sg::net::asio_io_pool]") {
    std::atomic<bool> called{false};

    {
        asio_io_pool::stopped_cb_t onStop = [&called](asio_io_pool&) {
            called.store(true);
            called.notify_all();
        };
        auto context = asio_io_pool::create(2, false, onStop);
        context->run();

    }

    called.wait(false);
}

TEST_CASE("asio_context_pool: callback called once", "[sg::net::asio_io_pool]") {
    std::atomic<int> count{0};

    {
        asio_io_pool::stopped_cb_t onStop = [&count](asio_io_pool&) {
            ++count;
            count.notify_all();
        };
        auto context = asio_io_pool::create(3, true, onStop);
        context->run();
    }

    REQUIRE(count == 1);
}


TEST_CASE("asio_context_pool: check stop_async() can be called from a context thread", "[sg::net::asio_io_pool]") {
    std::atomic<bool> called{false};

    asio_io_pool::stopped_cb_t onStop = [&called](asio_io_pool&) {
        called.store(true);
        called.notify_all();
    };
    auto context = asio_io_pool::create(2, false, onStop);

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard =
        boost::asio::make_work_guard(context->context());
    context->run();
    boost::asio::post(context->context(), [&context]() { context->stop_async(); });

    called.wait(false);
}

TEST_CASE("asio_context_pool: check pool is stopped after guard reset", "[sg::net::asio_io_pool]") {
    std::atomic<bool> called{false};

    asio_io_pool::stopped_cb_t onStop = [&called](asio_io_pool&) {
        called.store(true);
        called.notify_all();
    };
    auto context = asio_io_pool::create(2, false, onStop);
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard =
    boost::asio::make_work_guard(context->context());

    context->run();

    boost::asio::post(context->context(), [&guard]() { guard.reset(); });

    called.wait(false);
}

// TEST_CASE("asio_context_pool: running() a running context throws error", "[sg::net::asio_io_pool]") {
//     std::atomic<bool> called{false};
//
//     asio_io_pool::stopped_cb_t onStop = [&called](asio_io_pool&) {
//         called.store(true);
//         called.notify_all();
//     };
//     auto context = asio_io_pool::create(2, onStop);
//     boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard =
//     boost::asio::make_work_guard(context->context());
//
//     context->run();
//     REQUIRE_THROWS(context->run());
// }


TEST_CASE("asio_context_pool: you can stop guarded pool", "[sg::net::asio_io_pool]") {
    // Destructor
    {
        auto context = asio_io_pool::create(2, true, nullptr);
        context->run();
    }

    // Direct stop
    {
        auto context = asio_io_pool::create(2, true, nullptr);
        context->run();
        context->stop_async();
    }
}
//
//
// TEST_CASE("asio_context_pool: exception on get_future_once()", "[sg::net::asio_io_pool]") {
//     auto context = asio_io_pool::create(3, false, nullptr);
//
//     boost::asio::post(context->context(), []() { throw std::invalid_argument("tes"); });
//
//     context->run();
//     context->wait_for_stop();
//     REQUIRE_THROWS(context->future_get_once());
// }