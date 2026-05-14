#include <catch2/catch_test_macros.hpp>

#include "sg/exceptions.h"

#include <stdexcept>
#include <string>

namespace test_subsystem {
SG_CREATE_EXCEPTION_ANY();
SG_CREATE_EXCEPTION(time_out, "operation timed out");
SG_CREATE_EXCEPTION(other, "other error");
} // namespace test_subsystem

namespace test_other_subsystem {
SG_CREATE_EXCEPTION_ANY();
SG_CREATE_EXCEPTION(time_out, "operation timed out");
} // namespace test_other_subsystem

TEST_CASE("exceptions: SG_CREATE_EXCEPTION default constructor uses default description",
          "[sg::exceptions]") {
    test_subsystem::time_out e;
    REQUIRE(std::string{e.what()} == "operation timed out");
}

TEST_CASE("exceptions: SG_CREATE_EXCEPTION custom message constructor", "[sg::exceptions]") {
    test_subsystem::time_out e{"custom message"};
    REQUIRE(std::string{e.what()} == "custom message");
}

TEST_CASE("exceptions: SG_CREATE_EXCEPTION_ANY catches all subsystem exceptions",
          "[sg::exceptions]") {
    REQUIRE_THROWS_AS(throw test_subsystem::time_out{}, test_subsystem::any);
    REQUIRE_THROWS_AS(throw test_subsystem::other{}, test_subsystem::any);
}

TEST_CASE("exceptions: catching 'any' catches all libsg exceptions", "[sg::exceptions]") {
    REQUIRE_THROWS_AS(throw test_subsystem::time_out{}, sg::exceptions::any);
    REQUIRE_THROWS_AS(throw test_subsystem::other{}, sg::exceptions::any);
    REQUIRE_THROWS_AS(throw test_other_subsystem::time_out{}, sg::exceptions::any);
}

TEST_CASE("exceptions: specific exception does not catch sibling subsystem exception",
          "[sg::exceptions]") {
    try {
        throw test_other_subsystem::time_out{};
    } catch (const test_subsystem::any&) {
        FAIL("sibling subsystem caught wrong exception");
    } catch (const sg::exceptions::any&) {
        SUCCEED();
    }
}