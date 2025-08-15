#include "sg/environment.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("sg::environment check is_running_in_vm()") {
    REQUIRE_NOTHROW(sg::environment::is_running_in_vm());
}