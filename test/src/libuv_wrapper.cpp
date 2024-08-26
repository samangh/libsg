#include <doctest/doctest.h>

#include <sg/libuv_wrapper.h>

TEST_CASE("SG::common libuv_wrapper: check event callbacks are called") {

    SUBCASE("check wrap up and setup callbacks")
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

            sg::enable_lifetime_indicator ind;
            auto libuv = sg::libuv_wrapper();

            libuv.add_on_loop_started_cb(sg::create_weak_function(&ind, start_func));
            libuv.add_on_stopped_cb(sg::create_weak_function(&ind, stop_func));

            libuv.start_task(sg::create_weak_function(&ind, setup_func),
                             sg::create_weak_function(&ind, wrapup_func));

            CHECK_EQ(setup_cb_called_count, 1);
            CHECK_EQ(wrapup_cb_called_count, 0);
            CHECK_EQ(stop_cb_called_count, 0);
        }

        CHECK_EQ(start_cb_called_count, 1);
        CHECK_EQ(setup_cb_called_count, 1);
        CHECK_EQ(wrapup_cb_called_count, 1);
        CHECK_EQ(stop_cb_called_count, 1);
    }
}


TEST_CASE("SG::common libuv_wrapper: check libuv_wrapper.cpp(...) can be started/stopped with no tasks") {
    SUBCASE("simple initialisation") { auto libuv = sg::libuv_wrapper(); }

    SUBCASE("manual stop") {
        auto libuv = sg::libuv_wrapper();
        libuv.stop();
        CHECK(libuv.is_stopped());
    }

    SUBCASE("async stop") {
        auto libuv = sg::libuv_wrapper();
        libuv.stop_async();
        libuv.block_until_stopped();

        CHECK(libuv.is_stopped());
    }
}
