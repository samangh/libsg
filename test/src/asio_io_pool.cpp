#include <catch2/catch_test_macros.hpp>

#include "sg/net/asio_io_pool.h"

#include <boost/asio/post.hpp>
#include <thread>
#include <vector>

using namespace sg::net;

TEST_CASE("asio_io_pool: check unused context", "[sg::net::asio_io_pool]") {
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

TEST_CASE("asio_io_pool: check stop() calls callback", "[sg::net::asio_io_pool]") {
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

TEST_CASE("asio_io_pool: check destructor calls callback", "[sg::net::asio_io_pool]") {
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

TEST_CASE("asio_io_pool: callback called once", "[sg::net::asio_io_pool]") {
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


TEST_CASE("asio_io_pool: check stop_async() can be called from a context thread", "[sg::net::asio_io_pool]") {
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

TEST_CASE("asio_io_pool: check pool is stopped after guard reset", "[sg::net::asio_io_pool]") {
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

TEST_CASE("asio_io_pool: you can stop guarded pool", "[sg::net::asio_io_pool]") {
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

TEST_CASE("asio_io_pool: concurrent run/stop_async from multiple threads", "[sg::net::asio_io_pool]") {
    auto pool = asio_io_pool::create(2, true, nullptr);

    constexpr int iterations = 50;
    constexpr int num_threads = 4;
    std::vector<std::jthread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&pool, t]() {
            for (int i = 0; i < iterations; ++i) {
                if (t % 2 == 0)
                    pool->run();
                else
                    pool->stop_async();
            }
        });
    }

    threads.clear();

    pool->stop_async();
    pool->wait_for_stop();

    auto s = pool->state();
    REQUIRE((s == asio_io_pool::state_t::stopped));
}

TEST_CASE("asio_io_pool: restart under load", "[sg::net::asio_io_pool]") {
    auto pool = asio_io_pool::create(4, true, nullptr);
    pool->run();

    std::atomic<int> work_count{0};

    std::atomic<bool> keep_posting{true};
    auto poster = std::jthread([&]() {
        while (keep_posting.load()) {
            boost::asio::post(pool->context(), [&work_count]() {
                ++work_count;
                work_count.notify_all();
            });

            // Give the thread a tiny break
            std::this_thread::yield();
        }
    });

    for (int i = 0; i < 50; ++i) {
        pool->restart();
        REQUIRE(pool->is_running());
        int lastcount = work_count;
        work_count.wait(lastcount);
        REQUIRE(work_count > lastcount);
    }

    keep_posting.store(false);
    poster.join();

    pool->stop_async();
    pool->wait_for_stop();
}

TEST_CASE("asio_io_pool: callback invoked exactly once per cycle under stress", "[sg::net::asio_io_pool]") {
    constexpr int cycles = 20;
    std::atomic<int> cb_count{0};

    asio_io_pool::stopped_cb_t onStop = [&cb_count](asio_io_pool&) {
        ++cb_count;
    };

    auto pool = asio_io_pool::create(8, true, onStop);

    for (int i = 0; i < cycles; ++i) {
        pool->run();
        pool->stop_async();
        pool->wait_for_stop();
    }

    REQUIRE(cb_count.load() == cycles);
}

TEST_CASE("asio_io_pool: double stop_async is safe", "[sg::net::asio_io_pool]") {
    std::atomic<int> cb_count{0};

    asio_io_pool::stopped_cb_t onStop = [&cb_count](asio_io_pool&) {
        ++cb_count;
        cb_count.notify_all();
    };

    auto pool = asio_io_pool::create(4, true, onStop);
    pool->run();

    constexpr int num_threads = 8;
    std::vector<std::jthread> threads;

    for (int i = 0; i < num_threads; ++i)
        threads.emplace_back([&pool]() { pool->stop_async(); });

    threads.clear();
    cb_count.wait(0);

    REQUIRE(cb_count.load() == 1);
}

TEST_CASE("asio_io_pool: wait_for_stop without stop_async on guardless pool", "[sg::net::asio_io_pool]") {
    std::atomic<bool> handler_ran{false};

    auto pool = asio_io_pool::create(2, false, nullptr);

    boost::asio::post(pool->context(), [&handler_ran]() {
        handler_ran.store(true);
    });

    pool->run();
    pool->wait_for_stop();

    REQUIRE(handler_ran.load());
    REQUIRE(pool->state() == asio_io_pool::state_t::stopped);
}

TEST_CASE("asio_io_pool: wait_for_stop on never-started pool", "[sg::net::asio_io_pool]") {
    auto pool = asio_io_pool::create(2, true, nullptr);

    pool->wait_for_stop();

    REQUIRE(pool->state() == asio_io_pool::state_t::stopped);
    REQUIRE_FALSE(pool->is_running());
}

TEST_CASE("asio_io_pool: stop_async() from inside callback is a no-op", "[sg::net::asio_io_pool]") {
    std::atomic<bool> called{false};

    asio_io_pool::stopped_cb_t onStop = [&called](asio_io_pool& pool) {
        // documented as a no-op when called from the stopped callback;
        // must not deadlock on the internal mutex.
        pool.stop_async();
        called.store(true);
        called.notify_all();
    };

    auto pool = asio_io_pool::create(2, true, onStop);
    pool->run();
    pool->stop_async();
    pool->wait_for_stop();

    REQUIRE(called.load());
    REQUIRE(pool->state() == asio_io_pool::state_t::stopped);
}

TEST_CASE("asio_io_pool: wait_for_stop() from inside callback returns immediately", "[sg::net::asio_io_pool]") {
    std::atomic<bool> called{false};

    asio_io_pool::stopped_cb_t onStop = [&called](asio_io_pool& pool) {
        // documented to return immediately when called from the stopped
        // callback; the calling thread is the one that finalises the stop.
        pool.wait_for_stop();
        called.store(true);
        called.notify_all();
    };

    auto pool = asio_io_pool::create(2, true, onStop);
    pool->run();
    pool->stop_async();
    pool->wait_for_stop();

    REQUIRE(called.load());
    REQUIRE(pool->state() == asio_io_pool::state_t::stopped);
}

TEST_CASE("asio_io_pool: run() from inside callback throws logic_error", "[sg::net::asio_io_pool]") {
    std::atomic<bool> threw{false};

    asio_io_pool::stopped_cb_t onStop = [&threw](asio_io_pool& pool) {
        // an escaping exception from the callback would terminate the
        // program, so swallow the documented logic_error here.
        try {
            pool.run();
        } catch (const std::logic_error&) {
            threw.store(true);
            threw.notify_all();
        }
    };

    auto pool = asio_io_pool::create(2, true, onStop);
    pool->run();
    pool->stop_async();
    pool->wait_for_stop();

    REQUIRE(threw.load());
    REQUIRE(pool->state() == asio_io_pool::state_t::stopped);
}

TEST_CASE("asio_io_pool: restart() from inside callback throws logic_error", "[sg::net::asio_io_pool]") {
    std::atomic<bool> threw{false};

    asio_io_pool::stopped_cb_t onStop = [&threw](asio_io_pool& pool) {
        try {
            pool.restart();
        } catch (const std::logic_error&) {
            threw.store(true);
            threw.notify_all();
        }
    };

    auto pool = asio_io_pool::create(2, true, onStop);
    pool->run();
    pool->stop_async();
    pool->wait_for_stop();

    REQUIRE(threw.load());
    REQUIRE(pool->state() == asio_io_pool::state_t::stopped);
}
