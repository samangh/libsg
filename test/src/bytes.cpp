#include <sg/bytes.h>

#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("SG::common sg::bytes: check byteswap(...) family") {
    // see https://www.scadacore.com/tools/programming-calculators/online-hex-converter/

    /* check byteswap() function family */
    REQUIRE(0x0110 == sg::bytes::byteswap(uint16_t(0x1001)));
    REQUIRE((int16_t)0x0110 == sg::bytes::byteswap(int16_t(0x1001)));
    REQUIRE(0xEFBEADDE == sg::bytes::byteswap(uint32_t(0xDEADBEEFu)));
    REQUIRE((int32_t)0xEFBEADDE == sg::bytes::byteswap(int32_t(0xDEADBEEFu)));
    REQUIRE(0xEFCDAB8967452301 == sg::bytes::byteswap(uint64_t(0x0123456789ABCDEFull)));
    REQUIRE(int64_t(0xEFCDAB8967452301) == sg::bytes::byteswap(int64_t(0x0123456789ABCDEFull)));
}

TEST_CASE("SG::common sg::bytes: check to_bytes(...) and to_integral(...)") {
    /* check to_bytes */
    auto uint8_test = sg::bytes::to_bytes(uint8_t(0xAA));
    auto uint16_test = sg::bytes::to_bytes(uint16_t(0xAABB));
    auto uint32_test = sg::bytes::to_bytes(uint32_t(0xAABBCCDD));
    auto uint64_test = sg::bytes::to_bytes(uint64_t(0xAABBCCDDEEFF0011));
    REQUIRE(std::array<std::byte, 1>{std::byte{0xAA}} == uint8_test);
    REQUIRE(std::array<std::byte, 2>{std::byte{0xBB}, std::byte{0xAA}} == uint16_test);
    REQUIRE(std::array<std::byte, 4>{
                std::byte{0xDD}, std::byte{0xCC}, std::byte{0xBB}, std::byte{0xAA}} == uint32_test);
    REQUIRE(std::array<std::byte, 8>{std::byte{0x11},
                                     std::byte{0x00},
                                     std::byte{0xFF},
                                     std::byte{0xEE},
                                     std::byte{0xDD},
                                     std::byte{0xCC},
                                     std::byte{0xBB},
                                     std::byte{0xAA}} == uint64_test);

    /* check to_integral */
    REQUIRE(uint8_t(0xAA) ==
            sg::bytes::to_integral<uint8_t>(uint8_test.data(), std::endian::little));
    REQUIRE(uint8_t(0xAA) == sg::bytes::to_integral<uint8_t>(uint8_test.data(), std::endian::big));
    REQUIRE(uint16_t(0xAABB) ==
            sg::bytes::to_integral<uint16_t>(uint16_test.data(), std::endian::little));
    REQUIRE(uint16_t(0xBBAA) ==
            sg::bytes::to_integral<uint16_t>(uint16_test.data(), std::endian::big));
    REQUIRE(uint32_t(0xAABBCCDD) ==
            sg::bytes::to_integral<uint32_t>(uint32_test.data(), std::endian::little));
    REQUIRE(uint32_t(0xDDCCBBAA) ==
            sg::bytes::to_integral<uint32_t>(uint32_test.data(), std::endian::big));
    REQUIRE(uint64_t(0xAABBCCDDEEFF0011) ==
            sg::bytes::to_integral<uint64_t>(uint64_test.data(), std::endian::little));
    REQUIRE(uint64_t(0x1100FFEEDDCCBBAA) ==
            sg::bytes::to_integral<uint64_t>(uint64_test.data(), std::endian::big));
}

TEST_CASE("SG::common sg::bytes: check to_double(...) and from_bytes(double)") {
    REQUIRE(std::array<std::byte, 8>{std::byte{0x1B},
                                     std::byte{0xDE},
                                     std::byte{0x83},
                                     std::byte{0x42},
                                     std::byte{0xCA},
                                     std::byte{0xC0},
                                     std::byte{0xF3},
                                     std::byte{0x3F}} ==
            sg::bytes::to_bytes((double)1.23456789, std::endian::little));

    REQUIRE(std::array<std::byte, 8>{std::byte{0x3F},
                                     std::byte{0xF3},
                                     std::byte{0xC0},
                                     std::byte{0xCA},
                                     std::byte{0x42},
                                     std::byte{0x83},
                                     std::byte{0xDE},
                                     std::byte{0x1B}} ==
            sg::bytes::to_bytes((double)1.23456789, std::endian::big));
}
