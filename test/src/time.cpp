#include <catch2/catch_test_macros.hpp>

#include <sg/time.h>
#include <cmath>

TEST_CASE("sg::time: check to_epoch()", "[sg::time]") {
    REQUIRE(sg::time::to_epoch(sg::time::from_string("2025-01-01 01:02:00", "%F %T")) ==
            1735693320);
    REQUIRE(sg::time::to_epoch(sg::time::from_string("2025-01-01 01:02:00+00", "%F %T")) ==
            1735693320);
    REQUIRE(sg::time::to_epoch(sg::time::from_string("2025-01-01 01:02:00+01", "%F %T%z")) ==
            1735689720);
    REQUIRE(sg::time::to_epoch(sg::time::from_string("2025-01-01 01:02:00-01", "%F %T%z")) ==
            1735696920);
}

TEST_CASE("sg::time: check from_epoch()", "[sg::time]") {
    auto tp = sg::time::from_string("2025-01-01 01:02:00.1", "%F %T"); //1735693320
    REQUIRE(tp == sg::time::from_epoch(1735693320.1));
}