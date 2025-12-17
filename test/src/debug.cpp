#include "sg/debug.h"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_contains.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

#include <boost/stacktrace.hpp>
#include <iostream>

void test_throw(int a) {
    std::ignore = a;
    SG_THROW_EXCEPTION(std::runtime_error, "msg");
}

TEST_CASE("sg::common check THROW_DEBUG_EXCEPTION(..) macro") {
    REQUIRE_THROWS_AS(test_throw(2), std::runtime_error);
    REQUIRE_THROWS_WITH(test_throw(2), Catch::Matchers::ContainsSubstring("msg"));

    /* this checks that the trace is included in the message */
#ifdef LISBG_STACKTRACE
    REQUIRE_THROWS_WITH(test_throw(2), Catch::Matchers::ContainsSubstring("test_throw"));
#endif
}