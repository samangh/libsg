#include "sg/debug.h"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_contains.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

#include <iostream>

void test_throw(int a) {
    std::ignore = a;
    SG_THROW(std::runtime_error, "msg");
}

void test_throw_details(int a) {
    std::ignore = a;
    SG_THROW_DETAILS(std::runtime_error, "msg");
}

void test_catch(int a) {
    std::ignore = a;
    SG_CATCH_RETHROW(throw std::invalid_argument("hello from saman"));
}

TEST_CASE("sg::common check SG_CATCH_RETHROW(..) macro") {
#if defined(LIBSG_STACKTRACE) || defined(LIBSG_EXCEPTION_DETAILS)
    REQUIRE_THROWS_AS(test_catch(2), std::runtime_error);
#endif
    REQUIRE_THROWS_WITH(test_catch(2), Catch::Matchers::ContainsSubstring("hello from saman"));
}

TEST_CASE("sg::common check SG_THROW_...(..) macro") {
    /* SG THROW */
    REQUIRE_THROWS_AS(test_throw(2), std::runtime_error);
    REQUIRE_THROWS_WITH(test_throw(2), Catch::Matchers::ContainsSubstring("msg"));

    /* includes function name, file path */
    REQUIRE_THROWS_AS(test_throw_details(2), std::runtime_error);
    REQUIRE_THROWS_WITH(test_throw_details(2), Catch::Matchers::ContainsSubstring("test_throw_details"));
    REQUIRE_THROWS_WITH(test_throw_details(2), Catch::Matchers::ContainsSubstring("debug.cpp("));

#ifdef LIBSG_STACKTRACE
    /* includes stacktrace */
    REQUIRE_THROWS_WITH(test_throw(2), Catch::Matchers::ContainsSubstring("test_throw"));
    REQUIRE_THROWS_WITH(test_throw(2), Catch::Matchers::ContainsSubstring("debug.cp"));
#endif
}
