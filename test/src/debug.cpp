#include "sg/debug.h"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_contains.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

template<typename T>
static void test_throw(T a) {
    std::ignore = a;
    THROW_DEBUG_EXCEPTION(std::runtime_error, "msg");
}

TEST_CASE("sg::common check THROW_DEBUG_EXCEPTION(..) macro") {
    REQUIRE_THROWS_AS(test_throw(2), std::runtime_error);
    REQUIRE_THROWS_WITH( test_throw(2), Catch::Matchers::ContainsSubstring("msg"));
}