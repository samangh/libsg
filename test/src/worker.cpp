#include <catch2/catch_test_macros.hpp>
#include <semaphore>
#include <sg/worker.h>
#include <sg/notifiable_background_worker.h> // included for legacy testing of users

// TODO: add more tests around exception handling in started / stopped / action callbacks

TEST_CASE("worker: check start/stop callbacks get called in right order", "[sg::worker]") {
    std::atomic<int> counter{0};

    std::atomic<bool> start_called = false;
    std::atomic<bool> start_called_value_during_stop = false;

    sg::worker::on_tick_callback_t task = [&](sg::worker* w) {
        if (!start_called)
            w->request_stop();

        counter++;
        w->request_stop();
    };

    sg::worker::on_start_callback_t start_cb =
        [&start_called](sg::worker*) {
            start_called =true;
        };

    sg::worker::on_stop_callback_t stopped_cb =
        [&start_called, &start_called_value_during_stop](sg::worker*) {
            start_called_value_during_stop.store(start_called);
        };

    sg::worker worker =
        sg::worker(std::chrono::nanoseconds(100), task, start_cb, stopped_cb);
    worker.start();
    worker.wait_for_stop();

    REQUIRE(counter == 1);
    REQUIRE(start_called == true);
    REQUIRE(start_called_value_during_stop == 1);
}

TEST_CASE("worker: check worker can stop itself", "[sg::worker]") {
    std::atomic<int> counter{0};

    sg::worker::on_tick_callback_t task = [&](sg::worker* w) {
        counter++;
        w->request_stop();
    };

    sg::worker worker =
        sg::worker(std::chrono::nanoseconds(100), task, nullptr, nullptr);
    worker.start();
    worker.wait_for_stop();

    REQUIRE(counter == 1);
}

TEST_CASE("worker: check notifier works", "[sg::worker]") {
    std::binary_semaphore loop_1st_run{0};
    std::binary_semaphore loop_2nd_run{0};
    std::atomic<int> counter{0};

    sg::worker::on_tick_callback_t task = [&](sg::worker*) {
        counter++;
        loop_1st_run.release();
        if (counter == 2) {
            loop_2nd_run.release();
        }
    };

    sg::worker worker =
        sg::worker(std::chrono::seconds(10), task, nullptr, nullptr);
    worker.start();

    loop_1st_run.acquire();
    worker.notify();
    loop_2nd_run.acquire();
    worker.request_stop();
    worker.wait_for_stop();

    REQUIRE(counter == 2);
}

TEST_CASE("worker: check future() can throw errors", "[sg::worker]") {
    // Exception in main action
    {
        sg::worker::on_tick_callback_t taskWithExc =
            [&](sg::worker*) { throw std::runtime_error("hello!"); };

        sg::worker worker = sg::worker(
            std::chrono::milliseconds(100), taskWithExc, nullptr, nullptr);
        worker.start();

        REQUIRE_THROWS(worker.future().get());
        REQUIRE_THROWS(worker.future_get_once());
    }

    // Exception in stop cb
    {
        sg::worker::on_tick_callback_t emptyTask =
            [&](sg::worker*) {};

        sg::worker::on_stop_callback_t stopWithEx =
            [&](sg::worker*) { throw std::runtime_error("hello!"); };

        sg::worker worker = sg::worker(
            std::chrono::milliseconds(100), emptyTask, nullptr, stopWithEx);
        worker.start();
        worker.request_stop_after_iterations(1);
        worker.wait_for_stop();

        REQUIRE_THROWS(worker.future().get());
        REQUIRE_THROWS(worker.future_get_once());
    }
}

TEST_CASE("worker: check future_get_once() throws errors only once", "[sg::worker]") {
    std::binary_semaphore loop_run{0};
    std::atomic<int> counter{0};

    sg::worker::on_tick_callback_t task = [&](sg::worker*) {
        counter++;
        loop_run.release();
        throw std::runtime_error("err");
    };

    sg::worker worker =
        sg::worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
    worker.start();

    loop_run.acquire();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Worker will stop on it's own due to error

    REQUIRE_THROWS(worker.future_get_once());
    REQUIRE_NOTHROW(worker.future_get_once());
    REQUIRE(counter == 1);
}

TEST_CASE("worker: check destructor can throw an error", "[sg::worker]") {
    std::binary_semaphore loop_run{0};

    sg::worker::on_tick_callback_t task = [&](sg::worker*) {
        loop_run.release();
        throw std::runtime_error("err");
    };

    // Should not throw
    {
        loop_run.release(0);

        sg::worker worker =
            sg::worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
        worker.start();
        loop_run.acquire();
    };

    // Should throw

    {
        loop_run.release(0);

        sg::worker worker =
            sg::worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
        worker.start();
        loop_run.acquire();

        REQUIRE_THROWS(worker.future_get_once());
    };
}

TEST_CASE("worker: check you can't start multiple times", "[sg::worker]") {
    sg::worker::on_tick_callback_t task = [&](sg::worker*) { };

    sg::worker worker =
        sg::worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
    worker.start();

    CHECK_THROWS(worker.start());
}

TEST_CASE("worker: check is_stop_requested() whilst thread is busy", "[sg::worker]") {
    std::binary_semaphore sem{0}, sem_running{0};

    sg::worker::on_tick_callback_t task = [&](sg::worker*) {
        sem_running.release();
        sem.acquire();
    };

    sg::worker worker =
        sg::worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
    worker.set_interval(std::chrono::nanoseconds(1));

    worker.start();

    sem_running.acquire();
    REQUIRE(worker.is_running());

    worker.request_stop();
    REQUIRE(worker.stop_requested());

    sem.release();
}

TEST_CASE("worker: check you can start/stop multiple times", "[sg::worker]") {
    std::atomic<int> counter{0};

    sg::worker::on_tick_callback_t task = [&](sg::worker* w) {
        counter++;
        w->request_stop();
    };

    sg::worker worker =
        sg::worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
    worker.set_interval(std::chrono::nanoseconds(1));
    worker.start();
    worker.wait_for_stop();
    worker.start();
    worker.wait_for_stop();

    CHECK(counter==2);
}

TEST_CASE("worker: check request_stop_after_iterations(...) with non-zero input", "[sg::worker]") {
    std::atomic<int> counter{0};
    std::atomic<bool> requested{false};

    sg::worker::on_tick_callback_t task = [&](sg::worker* w) {
        counter++;
        if(!requested.exchange(true))
            w->request_stop_after_iterations(2);
    };

    sg::worker worker =
        sg::worker(std::chrono::nanoseconds(100), task, nullptr, nullptr);
    worker.start();
    worker.wait_for_stop();

    REQUIRE(counter == 3);
}

TEST_CASE("worker: check request_stop_after_iterations(...) with zero input", "[sg::worker]") {
    std::atomic<int> counter{0};
    std::atomic<bool> requested{false};

    sg::worker::on_tick_callback_t task = [&](sg::worker* w) {
        counter++;
        if(!requested.exchange(true))
            w->request_stop_after_iterations(0);
    };

    sg::worker worker =
        sg::worker(std::chrono::nanoseconds(100), task, nullptr, nullptr);
    worker.start();
    worker.wait_for_stop();

    REQUIRE(counter == 1);
}

TEST_CASE("worker: check start(...) will throw if the start callback has an error",
          "[sg::worker]") {
    std::atomic<int> counter{0};

    sg::worker::on_start_callback_t startCb = [&](sg::worker*) {
        throw std::runtime_error("catch!");
    };
    sg::worker::on_tick_callback_t task = [&](sg::worker*) {
        counter++;
    };
    sg::worker::on_stop_callback_t onStop = [&](sg::worker*) {
        counter++;
    };

    sg::worker worker =
        sg::worker(std::chrono::nanoseconds(100), task, startCb, onStop);

    REQUIRE_THROWS(worker.start());
    REQUIRE(counter == 00);
    REQUIRE(worker.is_running() == false);
}

TEST_CASE("worker: check call callbacks are done in the worker thread", "[sg::worker]") {
    // You can't use thread_id of a stopped thread, so I use a hash instead.

    std::hash<std::thread::id> hasher;
    auto this_thread_id = hasher(std::this_thread::get_id());
    std::atomic<size_t> startId, taskId, stopId;

    sg::worker::on_start_callback_t startCb = [&](sg::worker*) {
        startId = hasher(std::this_thread::get_id());
    };
    sg::worker::on_tick_callback_t task = [&](sg::worker*) {
        taskId = hasher(std::this_thread::get_id());
    };
    sg::worker::on_stop_callback_t stopCb = [&](sg::worker*) {
        stopId = hasher(std::this_thread::get_id());
    };

    sg::worker worker =
        sg::worker(std::chrono::nanoseconds(100), task, startCb, stopCb);

    worker.start();
    worker.request_stop_after_iterations(1);
    worker.wait_for_stop();

    REQUIRE(startId != this_thread_id);
    REQUIRE(startId == taskId);
    REQUIRE(startId == stopId);
}
