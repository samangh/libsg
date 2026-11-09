#include "sg/data/channel_rolling.h"
#include <catch2/catch_test_macros.hpp>



TEST_CASE("sg::data: channel_rolling: check from_bytes()", "[sg::data]") {
    sg::data::channel_rolling<int> chA = sg::data::channel_rolling<int>("", 10);
    int data[] = {1,2,3,4,5};
    chA.from_bytes(&data, sizeof(data));
    REQUIRE(chA.front() == 1);
    REQUIRE(chA.back() == 5);
}

TEST_CASE("sg::data: channel_rolling: check resize() reduction", "[sg::data]") {
    sg::data::channel_rolling<int> ch("", 5);
    ch.append({1, 2, 3, 4, 5});

    ch.resize(2);
    REQUIRE(ch.window_size() == 2);
    REQUIRE(ch.reserve_size() == 2);
    REQUIRE(ch.count() == 2);
    REQUIRE(ch.front() == 4);
    REQUIRE(ch.back() == 5);

    /* new size is honoured when rolling */
    ch.push_back(6);
    REQUIRE(ch.count() == 2);
    REQUIRE(ch.front() == 5);
    REQUIRE(ch.back() == 6);
}

TEST_CASE("sg::data: channel_rolling: check resize() growth", "[sg::data]") {
    sg::data::channel_rolling<int> ch("", 3);
    ch.append({1, 2, 3});

    ch.resize(5);
    REQUIRE(ch.window_size() == 5);
    REQUIRE(ch.reserve_size() == 5);
    REQUIRE(ch.count() == 3);

    ch.append({4, 5, 6});
    REQUIRE(ch.count() == 5);
    REQUIRE(ch.front() == 2);
    REQUIRE(ch.back() == 6);
}

TEST_CASE("sg::data: channel_rolling: check resize() with reserve", "[sg::data]") {
    sg::data::channel_rolling<int> ch("", 3);
    ch.append({1, 2, 3});

    ch.resize(4, 1);
    REQUIRE(ch.window_size() == 4);
    REQUIRE(ch.reserve_size() == 1);
    REQUIRE(ch.count() == 3);

    /* roll past the reserve to force the buffer to shift */
    ch.append({4, 5, 6, 7});
    REQUIRE(ch.count() == 4);
    REQUIRE(ch.front() == 4);
    REQUIRE(ch.back() == 7);
}
