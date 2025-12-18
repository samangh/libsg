#include "sg/process.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("sg::process check is_hypervisor_flag_set()", "[sg::process]") {
    REQUIRE_NOTHROW(sg::process::get_processes());
}
