#include <doctest/doctest.h>

#include <sg/libuv_wrapper.h>

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
