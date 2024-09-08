#include <sg/crc.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("SG::common sg::cheksum: check crc functions") {
    // see https://crccalc.com/

    std::string input = "123456789";
    auto crc16 = sg::checksum::crc16(input.c_str(), input.length());
    auto crc32 = sg::checksum::crc32(input.c_str(), input.length());
    auto crc32c = sg::checksum::crc32c(input.c_str(), input.length());

    REQUIRE(crc16 == 0xBB3D);
    REQUIRE(crc32 == 0xCBF43926);
    REQUIRE(crc32c == 0xE3069283);
}
