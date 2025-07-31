#include "sg/cpu.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("sg::common::cpu check is_hypervisor_flag_set()") {
    REQUIRE_NOTHROW(sg::cpu::is_hypervisor_flag_set());
}
