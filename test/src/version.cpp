#include <catch2/catch_test_macros.hpp>

#include <sg/version.h>

TEST_CASE("sg::version check constructors", "[sg::version]") {
    sg::version ver;
    ver.versions={1,2,3,4,15};
    REQUIRE(sg::version("1.2.3.4.15") == ver);
    REQUIRE(sg::version({1,2,3,4,15}) == ver);

    REQUIRE_THROWS(sg::version("1.2.3.1v"));
    REQUIRE_THROWS(sg::version("v"));
}

TEST_CASE("sg::version check operator!=()", "[sg::version]") {
    REQUIRE(sg::version({1,2,3}) == sg::version({1,2,3}));
    REQUIRE(sg::version({1,2,3}) != sg::version({1,2,3,4}));
}

TEST_CASE("sg::version check operator<=>()", "[sg::version]") {
    REQUIRE(sg::version({1,2,3}) > sg::version({1,2,2}));
    REQUIRE(sg::version({1,2,3}) >= sg::version({1,2,2}));
    REQUIRE(sg::version({1,2,3}) >= sg::version({1,2,3}));
    REQUIRE(sg::version({1,2,2}) <= sg::version({1,2,3}));
    REQUIRE(sg::version({1,2,0}) > sg::version({1,2}));
}

TEST_CASE("sg::version check operator std::string() ", "[sg::version]") {
    REQUIRE(static_cast<std::string>(sg::version({1, 2, 3})) == "1.2.3");
}

TEST_CASE("sg::version check operator<<(...) ", "[sg::version]") {
    std::stringstream ss;
    ss << sg::version({1, 2, 3});

    REQUIRE(ss.str() == "1.2.3");
}
