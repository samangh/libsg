#include "sg/cpu.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("sg::common::cpu check current_cpu_vendor()") {
    REQUIRE_NOTHROW(sg::cpu::current_cpu_vendor());
}

TEST_CASE("sg::common::cpu check info()") {
    auto info = sg::cpu::info();
    REQUIRE(!info.empty());
}

TEST_CASE("sg::common::cpu check available_parallelism()") {
    REQUIRE_NOTHROW(sg::cpu::available_parallelism());
}
