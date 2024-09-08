#include <catch2/catch_test_macros.hpp>

#include <sg/libuv_wrapper.h>

TEST_CASE("SG::common libuv_wrapper: check callbacks are called") {

    SECTION("check wrap up and setup callbacks")
    {
        std::atomic_int wrapup_cb_called_count{0};
        std::atomic_int setup_cb_called_count{0};
        std::atomic_int start_cb_called_count{0};
        std::atomic_int stop_cb_called_count{0};
        {
            std::function<sg::libuv_wrapper::wrapup_result(sg::libuv_wrapper *)> wrapup_func =
                [&](sg::libuv_wrapper *) {
                    wrapup_cb_called_count.fetch_add(1);
                    return sg::libuv_wrapper::wrapup_result::stop_uv_loop;
                };
            std::function<void(sg::libuv_wrapper *)> setup_func = [&](sg::libuv_wrapper *) {
                setup_cb_called_count.fetch_add(1);
            };
            std::function<void(sg::libuv_wrapper *)> start_func = [&](sg::libuv_wrapper *) {
                start_cb_called_count.fetch_add(1);
            };
            std::function<void(sg::libuv_wrapper *)> stop_func = [&](sg::libuv_wrapper *) {
                stop_cb_called_count.fetch_add(1);
            };

            auto libuv = sg::libuv_wrapper();

            libuv.add_on_loop_started_cb(start_func);
            libuv.add_on_stopped_cb(stop_func);

            libuv.start_task(setup_func, wrapup_func);

            REQUIRE(setup_cb_called_count== 1);
            REQUIRE(wrapup_cb_called_count== 0);
            REQUIRE(stop_cb_called_count== 0);
        }

        REQUIRE(start_cb_called_count== 1);
        REQUIRE(setup_cb_called_count== 1);
        REQUIRE(wrapup_cb_called_count== 1);
        REQUIRE(stop_cb_called_count== 1);
    }

    SECTION("check start/stop callbacks can be removed"){
        std::atomic_int start_cb_called_count{0};
        std::atomic_int stop_cb_called_count{0};
        {
            std::function<void(sg::libuv_wrapper *)> start_func = [&](sg::libuv_wrapper *) {
                start_cb_called_count.fetch_add(1);
            };

            auto libuv = sg::libuv_wrapper();

            auto start_cb_index = libuv.add_on_loop_started_cb(
                [&](sg::libuv_wrapper *) { start_cb_called_count.fetch_add(1); });
            auto stop_cb_index = libuv.add_on_stopped_cb(
                [&](sg::libuv_wrapper *) { stop_cb_called_count.fetch_add(1); });

            libuv.remove_task_callbacks(start_cb_index);
            libuv.start_task(nullptr, nullptr);
            libuv.remove_task_callbacks(stop_cb_index);
        }
        REQUIRE(start_cb_called_count == 0);
        REQUIRE(stop_cb_called_count== 0);

    }
}


TEST_CASE("SG::common libuv_wrapper: check libuv_wrapper.cpp(...) can be started/stopped with no tasks") {
    SECTION("simple initialisation") { auto libuv = sg::libuv_wrapper(); }

    SECTION("manual stop") {
        auto libuv = sg::libuv_wrapper();
        libuv.stop();
        CHECK(libuv.is_stopped());
    }

    SECTION("async stop") {
        auto libuv = sg::libuv_wrapper();
        libuv.stop_async();
        libuv.block_until_stopped();

        CHECK(libuv.is_stopped());
    }
}
