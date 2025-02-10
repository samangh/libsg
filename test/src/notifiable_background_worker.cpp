#include <sg/notifiable_background_worker.h>

#include <catch2/catch_test_macros.hpp>
#include <semaphore>
#include <string>

//TODO: add more tests aroudn exception handling in started / stopped / action callbacks

TEST_CASE("SG::notifiable_background_worker: check start/stop callbacks get called in right order",
          "[SG::notifiable_background_worker]") {
    std::binary_semaphore sem{0};
    std::binary_semaphore loop_run{0};
    std::atomic<int> counter{-1};
    std::atomic<int> at_stop_value{0};

    std::atomic<bool> ran_already = false;

    sg::notifiable_background_worker::callback_t task = [&](sg::notifiable_background_worker*) {
        // Ensure we run once only
        if (ran_already.exchange(true)) return;

        // counter is -1 here
        sem.acquire();
        // counter is 0 after the semaphore is acquired
        counter++;
        // counter is +1 here

        loop_run.release();
    };

    sg::notifiable_background_worker::callback_t start_cb =
        [&counter, &sem](sg::notifiable_background_worker*) {
            counter++;
            // counter should be incremented to 0 here
            sem.release();
        };

    sg::notifiable_background_worker::callback_t stopped_cb =
        [&at_stop_value, &counter](sg::notifiable_background_worker*) {
            at_stop_value.store(counter);
        };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(100), task, start_cb, stopped_cb);
    worker.start();

    /* Need to make sure we run at least one loop - wait until loop is rune once then stop */
    loop_run.acquire();
    worker.request_stop();
    worker.wait_for_stop();

    REQUIRE(at_stop_value == 1);
}

TEST_CASE("SG::notifiable_background_worker: check worker can stop itself",
          "[SG::notifiable_background_worker]") {
    std::atomic<int> counter{0};

    sg::notifiable_background_worker::callback_t task = [&](sg::notifiable_background_worker* w) {
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

    sg::notifiable_background_worker::callback_t task = [&](sg::notifiable_background_worker*) {
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

TEST_CASE("SG::notifiable_background_worker: check exception handling",
          "[SG::notifiable_background_worker]") {
    std::binary_semaphore loop_run{0};
    std::atomic<int> counter{0};

    sg::notifiable_background_worker::callback_t task = [&](sg::notifiable_background_worker*) {
        counter++;
        loop_run.release();
        throw std::runtime_error("err");
    };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
    worker.start();

    loop_run.acquire();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Don't call stop_request() - the worker should stop on it's own
    worker.wait_for_stop();

    REQUIRE(counter == 1);
    CHECK_THROWS(worker.future().get());
}


TEST_CASE("SG::notifiable_background_worker: check you can't start multiple times",
          "[SG::notifiable_background_worker]") {

    sg::notifiable_background_worker::callback_t task = [&](sg::notifiable_background_worker*) {

    };

    sg::notifiable_background_worker worker =
        sg::notifiable_background_worker(std::chrono::nanoseconds(10), task, nullptr, nullptr);
    worker.start();

    CHECK_THROWS(worker.start());
}

TEST_CASE("SG::notifiable_background_worker: check is_stop_requested() whilst thread is busy",
          "[SG::notifiable_background_worker]") {
    std::binary_semaphore sem{0}, sem_running{0};

    sg::notifiable_background_worker::callback_t task = [&](sg::notifiable_background_worker*) {
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

    sg::notifiable_background_worker::callback_t task = [&](sg::notifiable_background_worker* w) {
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
