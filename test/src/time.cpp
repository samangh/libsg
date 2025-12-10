#include <catch2/catch_test_macros.hpp>

#include <sg/time.h>
#include <cmath>

TEST_CASE("sg::time: check to_epoch() ", "[sg::time]") {
    REQUIRE(sg::time::to_epoch(sg::time::from_string("2025-01-01 01:02:00", "%F %T")) ==
            1735693320);
    REQUIRE(sg::time::to_epoch(sg::time::from_string("2025-01-01 01:02:00+00", "%F %T")) ==
            1735693320);
    REQUIRE(sg::time::to_epoch(sg::time::from_string("2025-01-01 01:02:00+01", "%F %T%z")) ==
            1735689720);
    REQUIRE(sg::time::to_epoch(sg::time::from_string("2025-01-01 01:02:00-01", "%F %T%z")) ==
            1735696920);
}

TEST_CASE("sg::time: check from_epoch() and from_string()", "[sg::time]") {
    /* note:
     *  - number of characters to read for seconds can be specified as %nS
     *  - On Windows, if you ask for mode digits than the resolution allows for you may get an
     *    exception */

    // Default parameters
    {
        auto tp = sg::time::from_string("2025-01-01 01:02:01.1", "%F %T"); //1735693320
        REQUIRE(tp == sg::time::from_epoch(1735693321.1));
    }

    /* Resolution set to 1s */
    {
        typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> tp_t;
        auto tp = sg::time::from_string<tp_t>("2025-01-01 01:02:01.1", "%F %H:%M:%4S");
        REQUIRE(tp==sg::time::from_epoch<tp_t>(1735693321));
    }


}