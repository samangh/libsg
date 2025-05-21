#include <sg/gettimeofday.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("sg::common gettimeofday(...)", "[gettimeofday]") {
    timeval timeval_{};

    gettimeofday(&timeval_, nullptr);
    REQUIRE(timeval_.tv_sec!=0);
}
