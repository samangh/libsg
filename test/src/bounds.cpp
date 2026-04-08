#include <catch2/catch_test_macros.hpp>

#include "sg/bounds.h"

TEST_CASE("bounds: greater_or_equal_index(...)", "[sg::bounds]") {
    std::vector<int> vec{0,1,2,2,3,4};

    REQUIRE(sg::bounds::greater_or_equal_index(vec.data(), vec.size(), 2) == 3);
    REQUIRE(sg::bounds::greater_or_equal_index(vec.data(), vec.size(), 3) == 4);
    REQUIRE(sg::bounds::greater_or_equal_index(vec.data(), vec.size(), 6) == 5);
}