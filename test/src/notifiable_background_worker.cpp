#include <sg/notifiable_background_worker.h>

#include <catch2/catch_test_macros.hpp>
#include <semaphore>

//TODO: add more tests aroudn exception handling in started / stopped / action callbacks

TEST_CASE("SG::notifiable_background_worker: check start/stop callbacks get called in right order",
          "[SG::notifiable_background_worker]") {
    std::atomic<int> counter{0};

    std::atomic<bool> start_called = false;
    std::atomic<bool> start_called_value_during_stop = false;

    sg::notifiable_background_worker::on_tick_callback_t task = [&](sg::notifiable_background_worker* w) {
        if (!start_called)
            w->request_stop();

        counter++;
        w->request_stop();
    };

    sg::notifiable_background_worker::on_start_callback_t start_cb =
        [&start_called](sg::notifiable_background_worker*) {
            start_called =true;
        };

    sg::notifiable_background_worker::on_stop_callback_t stopped_cb =
        [&start_called, &start_called_value_during_stop](sg::notifiable_background_worker*) {
            start_called_value_during_stop.store(start_called);
        };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(100), task, start_cb, stopped_cb);
    worker.start();
    worker.wait_for_stop();

    REQUIRE(counter == 1);
    REQUIRE(start_called == true);
    REQUIRE(start_called_value_during_stop == 1);
}

TEST_CASE("SG::notifiable_background_worker: check worker can stop itself",
          "[SG::notifiable_background_worker]") {
    std::atomic<int> counter{0};

    sg::notifiable_background_worker::on_tick_callback_t task = [&](sg::notifiable_background_worker* w) {
        counter++;
        w->request_stop();
    };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(100), task, nullptr, nullptr);
    worker.start();
    worker.wait_for_stop();

    REQUIRE(counter == 1);
}

TEST_CASE("SG::notifiable_background_worker: check notifier works",
          "[SG::notifiable_background_worker]") {
    std::binary_semaphore loop_1st_run{0};
    std::binary_semaphore loop_2nd_run{0};
    std::atomic<int> counter{0};

    sg::notifiable_background_worker::on_tick_callback_t task = [&](sg::notifiable_background_worker*) {
        counter++;
        loop_1st_run.release();
        if (counter == 2) {
            loop_2nd_run.release();
        }
    };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::seconds(10), task, nullptr, nullptr);
    worker.start();

    loop_1st_run.acquire();
    worker.notify();
    loop_2nd_run.acquire();
    worker.request_stop();
    worker.wait_for_stop();

    REQUIRE(counter == 2);
}

TEST_CASE("SG::notifiable_background_worker: check future() can throw errors",
          "[SG::notifiable_background_worker]") {


    // Exception in main action
    {
        sg::notifiable_background_worker::on_tick_callback_t taskWithExc =
            [&](sg::notifiable_background_worker*) { throw std::runtime_error("hello!"); };

        sg::notifiable_background_worker worker = sg::notifiable_background_worker(
            std::chrono::milliseconds(100), taskWithExc, nullptr, nullptr);
        worker.start();

        REQUIRE_THROWS(worker.future().get());
        REQUIRE_THROWS(worker.future_get_once());
    }

    // Exception in stop cb
    {
        sg::notifiable_background_worker::on_tick_callback_t emptyTask =
            [&](sg::notifiable_background_worker*) {};

        sg::notifiable_background_worker::on_stop_callback_t stopWithEx =
            [&](sg::notifiable_background_worker*) { throw std::runtime_error("hello!"); };

        sg::notifiable_background_worker worker = sg::notifiable_background_worker(
            std::chrono::milliseconds(100), emptyTask, nullptr, stopWithEx);
        worker.start();
        worker.request_stop_after_iterations(1);
        worker.wait_for_stop();

        REQUIRE_THROWS(worker.future().get());
        REQUIRE_THROWS(worker.future_get_once());
    }
}

TEST_CASE("SG::notifiable_background_worker: check future_get_once() throws errors only once",
          "[SG::notifiable_background_worker]") {
    std::binary_semaphore loop_run{0};
    std::atomic<int> counter{0};

    sg::notifiable_background_worker::on_tick_callback_t task = [&](sg::notifiable_background_worker*) {
        counter++;
        loop_run.release();
        throw std::runtime_error("err");
    };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
    worker.start();

    loop_run.acquire();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Worker will stop on it's own due to error

    REQUIRE_THROWS(worker.future_get_once());
    REQUIRE_NOTHROW(worker.future_get_once());
    REQUIRE(counter == 1);
}

TEST_CASE("SG::notifiable_background_worker: check destructor can throw an error",
          "[SG::notifiable_background_worker]") {
    std::binary_semaphore loop_run{0};

    sg::notifiable_background_worker::on_tick_callback_t task = [&](sg::notifiable_background_worker*) {
        loop_run.release();
        throw std::runtime_error("err");
    };

    // Should throw
    REQUIRE_THROWS([&](){
        loop_run.release(0);

        sg::notifiable_background_worker worker =
            sg::notifiable_background_worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
        worker.start();
        loop_run.acquire();
    }());

    // Should not throw
    REQUIRE_NOTHROW([&](){
        loop_run.release(0);

        sg::notifiable_background_worker worker =
            sg::notifiable_background_worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
        worker.start();
        loop_run.acquire();

        try {
        worker.future_get_once();
        } catch(...){}
    }());
}

TEST_CASE("SG::notifiable_background_worker: check you can't start multiple times",
          "[SG::notifiable_background_worker]") {

    sg::notifiable_background_worker::on_tick_callback_t task = [&](sg::notifiable_background_worker*) {

    };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
    worker.start();

    CHECK_THROWS(worker.start());
}

TEST_CASE("SG::notifiable_background_worker: check is_stop_requested() whilst thread is busy",
          "[SG::notifiable_background_worker]") {
    std::binary_semaphore sem{0}, sem_running{0};

    sg::notifiable_background_worker::on_tick_callback_t task = [&](sg::notifiable_background_worker*) {
        sem_running.release();
        sem.acquire();
    };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
    worker.set_interval(std::chrono::nanoseconds(1));

    worker.start();

    sem_running.acquire();
    REQUIRE(worker.is_running());

    worker.request_stop();
    REQUIRE(worker.stop_requested());

    sem.release();
}

TEST_CASE("SG::notifiable_background_worker: check you can start/stop multiple times",
          "[SG::notifiable_background_worker]") {
    std::atomic<int> counter{0};

    sg::notifiable_background_worker::on_tick_callback_t task = [&](sg::notifiable_background_worker* w) {
        counter++;
        w->request_stop();
    };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
    worker.set_interval(std::chrono::nanoseconds(1));
    worker.start();
    worker.wait_for_stop();
    worker.start();
    worker.wait_for_stop();

    CHECK(counter==2);
}

TEST_CASE("SG::notifiable_background_worker: check request_stop_after_iterartions(...) with non-zero input",
          "[SG::notifiable_background_worker]") {
    std::atomic<int> counter{0};
    std::atomic<bool> requested{false};

    sg::notifiable_background_worker::on_tick_callback_t task = [&](sg::notifiable_background_worker* w) {
        counter++;
        if(!requested.exchange(true))
            w->request_stop_after_iterations(2);
    };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(100), task, nullptr, nullptr);
    worker.start();
    worker.wait_for_stop();

    REQUIRE(counter == 3);
}

TEST_CASE("SG::notifiable_background_worker: check request_stop_after_iterartions(...) with zero input",
          "[SG::notifiable_background_worker]") {
    std::atomic<int> counter{0};
    std::atomic<bool> requested{false};

    sg::notifiable_background_worker::on_tick_callback_t task = [&](sg::notifiable_background_worker* w) {
        counter++;
        if(!requested.exchange(true))
            w->request_stop_after_iterations(0);
    };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(100), task, nullptr, nullptr);
    worker.start();
    worker.wait_for_stop();

    REQUIRE(counter == 1);
}

TEST_CASE("SG::notifiable_background_worker: check start(...) will throw if the start callback has an error",
          "[SG::notifiable_background_worker]") {
    std::atomic<int> counter{0};

    sg::notifiable_background_worker::on_start_callback_t startCb = [&](sg::notifiable_background_worker*) {
        throw std::runtime_error("catch!");
    };
    sg::notifiable_background_worker::on_tick_callback_t task = [&](sg::notifiable_background_worker*) {
        counter++;
    };
    sg::notifiable_background_worker::on_stop_callback_t onStop = [&](sg::notifiable_background_worker*) {
        counter++;
    };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(100), task, startCb, onStop);

    REQUIRE_THROWS(worker.start());
    REQUIRE(counter == 00);
    REQUIRE(worker.is_running() == false);
}

TEST_CASE("SG::notifiable_background_worker: check call callbacks are done in the worker thread",
          "[SG::notifiable_background_worker]") {

    // You can't use thread_id of a stopped thread, so I use a hash instead.

    std::hash<std::thread::id> hasher;
    auto this_thread_id = hasher(std::this_thread::get_id());
    std::atomic<size_t> startId, taskId, stopId;

    sg::notifiable_background_worker::on_start_callback_t startCb = [&](sg::notifiable_background_worker*) {
        startId = hasher(std::this_thread::get_id());
    };
    sg::notifiable_background_worker::on_tick_callback_t task = [&](sg::notifiable_background_worker*) {
        taskId = hasher(std::this_thread::get_id());
    };
    sg::notifiable_background_worker::on_stop_callback_t stopCb = [&](sg::notifiable_background_worker*) {
        stopId = hasher(std::this_thread::get_id());
    };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(100), task, startCb, stopCb);

    worker.start();
    worker.request_stop_after_iterations(1);
    worker.wait_for_stop();

    REQUIRE(startId != this_thread_id);
    REQUIRE(startId == taskId);
    REQUIRE(startId == stopId);
}
