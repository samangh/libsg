#include "sg/hardware.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("sg::common::hardware check is_running_in_vm()") {
    REQUIRE_NOTHROW(sg::common::hardware::is_running_in_vm());
}

TEST_CASE("sg::common::hardware check bios_vendor()") {
    REQUIRE_NOTHROW(sg::common::hardware::bios_vendor());
}