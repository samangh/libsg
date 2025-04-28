#include <sg/ranges.h>

#include <catch2/catch_test_macros.hpp>


TEST_CASE("sg::common::ranges erase_value(...)", "[sg::common::ranges]") {
    std::vector<int> vec{0,1,2,2,3,3,3};

    sg::ranges::erase_value(vec, 1);
    REQUIRE(vec == std::vector<int> {0,2,2,3,3,3});

    sg::ranges::erase_value(vec, 2);
    REQUIRE(vec == std::vector<int> {0,3,3,3});

    sg::ranges::erase_value(vec, 100);
    REQUIRE(vec == std::vector<int> {0,3,3,3});}

TEST_CASE("sg::common::ranges erase_value_if(...)", "[sg::common::ranges]") {
    std::vector<int> vec{0,1,2,2,3,3,3};

    sg::ranges::erase_value_if(vec, [](const auto& v){return v==1;});
    REQUIRE(vec == std::vector<int> {0,2,2,3,3,3});

    sg::ranges::erase_value_if(vec, [](const auto& v){return v==2;});
    REQUIRE(vec == std::vector<int> {0,3,3,3});

    sg::ranges::erase_value_if(vec, [](const auto& v){return v==100;});
    REQUIRE(vec == std::vector<int> {0,3,3,3});
}
