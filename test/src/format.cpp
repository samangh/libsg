#include <sg/format.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

TEST_CASE("sg::common sg::format: check to_hex(...) family", "[sg::format]") {
    const std::vector<std::byte> byte_vec{std::byte{0x01},
                                          std::byte{0x02},
                                          std::byte{0x03},
                                          std::byte{0xAA},
                                          std::byte{0xBB},
                                          std::byte{0xFF}};
    const std::array<std::byte, 6> byte_arr{std::byte{0x01},
                                            std::byte{0x02},
                                            std::byte{0x03},
                                            std::byte{0xAA},
                                            std::byte{0xBB},
                                            std::byte{0xFF}};

    /* check for passing vector or array of bytes */
    REQUIRE("010203AABBFF" == sg::format::to_hex(byte_vec, ""));
    REQUIRE("010203AABBFF" == sg::format::to_hex(byte_arr, ""));

    /* check iterator */
    REQUIRE("010203AABBFF" == sg::format::to_hex(byte_vec.begin(), byte_vec.end(), ""));
    REQUIRE("010203AABBFF" == sg::format::to_hex(byte_arr.begin(), byte_arr.end(), ""));

    /* check const iterator */
    REQUIRE("010203AABBFF" == sg::format::to_hex(byte_vec.cbegin(), byte_vec.cend(), ""));
    REQUIRE("010203AABBFF" == sg::format::to_hex(byte_arr.cbegin(), byte_arr.cend(), ""));

    /* check direct object conversion */
    REQUIRE("01" == sg::format::to_hex(uint8_t(0x01)));
    REQUIRE("0100" == sg::format::to_hex(uint16_t(0x0001)));
    REQUIRE("01000000" == sg::format::to_hex(uint32_t(0x00000001)));
    REQUIRE("AA" == sg::format::to_hex(uint8_t(0xAA)));
    REQUIRE("BBAA" == sg::format::to_hex(uint16_t(0xAABB)));
    REQUIRE("DDCCBBAA" == sg::format::to_hex(uint32_t(0xAABBCCDD)));
    REQUIRE("1100FFEEDDCCBBAA" == sg::format::to_hex(uint64_t(0xAABBCCDDEEFF0011)));

    /* check separat makes no effect for bytes within an object */
    REQUIRE("11 00 FF EE DD CC BB AA" == sg::format::to_hex(uint64_t(0xAABBCCDDEEFF0011), " "));

    /* check objects that can be trivally be converted to std::byte */
    std::array<uint8_t, 4> int_arr{(uint8_t)0x01, (uint8_t)0x02, (uint8_t)0x03, (uint8_t)0x04};
    std::array<char, 4> char_arr{(char)0x01, (char)0x02, (char)0x03, (char)0x04};
    REQUIRE("01020304" == sg::format::to_hex(int_arr));
    REQUIRE("01020304" == sg::format::to_hex(char_arr));
}
