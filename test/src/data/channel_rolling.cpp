#include "sg/data/channel_rolling.h"
#include <catch2/catch_test_macros.hpp>



TEST_CASE("sg::data: channel_rolling: check from_bytes()", "[sg::data]") {
    sg::data::channel_rolling<int> chA = sg::data::channel_rolling<int>("", 10);
    int data[] = {1,2,3,4,5};
    chA.from_bytes(&data, sizeof(data));
    REQUIRE(chA.front() == 1);
    REQUIRE(chA.back() == 5);
}