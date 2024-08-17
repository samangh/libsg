#include <sg/bytes.h>

#include <doctest/doctest.h>

#include <string>

TEST_CASE("SG::common sg::bytes: check byteswap(...) family") {
    // see https://www.scadacore.com/tools/programming-calculators/online-hex-converter/

    /* check byteswap() function family */
    CHECK_EQ(0x0110, sg::bytes::byteswap(uint16_t(0x1001)));
    CHECK_EQ(0x0110, sg::bytes::byteswap(int16_t(0x1001)));
    CHECK_EQ(0xEFBEADDE, sg::bytes::byteswap(uint32_t(0xDEADBEEFu)));
    CHECK_EQ(0xEFBEADDE, sg::bytes::byteswap(int32_t(0xDEADBEEFu)));
    CHECK_EQ(0xEFCDAB8967452301, sg::bytes::byteswap(uint64_t(0x0123456789ABCDEFull)));
    CHECK_EQ(0xEFCDAB8967452301, sg::bytes::byteswap(int64_t(0x0123456789ABCDEFull)));
}

TEST_CASE("SG::common sg::bytes: check to_bytes(...) and to_integral(...)") {
    /* check to_bytes */
    auto uint8_test = sg::bytes::to_bytes(uint8_t(0xAA));
    auto uint16_test = sg::bytes::to_bytes(uint16_t(0xAABB));
    auto uint32_test = sg::bytes::to_bytes(uint32_t(0xAABBCCDD));
    auto uint64_test = sg::bytes::to_bytes(uint64_t(0xAABBCCDDEEFF0011));
    CHECK_EQ(std::array<std::byte, 1>{std::byte{0xAA}}, uint8_test);
    CHECK_EQ(std::array<std::byte, 2>{std::byte{0xBB}, std::byte{0xAA}},
             uint16_test);
    CHECK_EQ(std::array<std::byte, 4>{std::byte{0xDD}, std::byte{0xCC},
                                      std::byte{0xBB}, std::byte{0xAA}},
             uint32_test);
    CHECK_EQ(std::array<std::byte, 8>{std::byte{0x11}, std::byte{0x00},
                                      std::byte{0xFF}, std::byte{0xEE},
                                      std::byte{0xDD}, std::byte{0xCC},
                                      std::byte{0xBB}, std::byte{0xAA}},
             uint64_test);

    /* check to_integral */
    CHECK_EQ(uint8_t(0xAA), sg::bytes::to_integral<uint8_t>(uint8_test.data(), std::endian::little));
    CHECK_EQ(uint8_t(0xAA), sg::bytes::to_integral<uint8_t>(uint8_test.data(), std::endian::big));
    CHECK_EQ(uint16_t(0xAABB), sg::bytes::to_integral<uint16_t>(uint16_test.data(), std::endian::little));
    CHECK_EQ(uint16_t(0xBBAA), sg::bytes::to_integral<uint16_t>(uint16_test.data(), std::endian::big));
    CHECK_EQ(uint32_t(0xAABBCCDD), sg::bytes::to_integral<uint32_t>(uint32_test.data(), std::endian::little));
    CHECK_EQ(uint32_t(0xDDCCBBAA), sg::bytes::to_integral<uint32_t>(uint32_test.data(), std::endian::big));
    CHECK_EQ(uint64_t(0xAABBCCDDEEFF0011), sg::bytes::to_integral<uint64_t>(uint64_test.data(), std::endian::little));
    CHECK_EQ(uint64_t(0x1100FFEEDDCCBBAA), sg::bytes::to_integral<uint64_t>(uint64_test.data(), std::endian::big));
}
