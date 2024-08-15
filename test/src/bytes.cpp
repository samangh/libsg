#include <sg/bytes.h>

#include <doctest/doctest.h>

#include <string>

TEST_CASE("SG::common sg::bytes: check to_uint16()") {
    // see https://www.scadacore.com/tools/programming-calculators/online-hex-converter/

    {
        uint8_t data[2] = {0x00, 0x01};
        uint16_t little = sg::bytes::to_integral<uint16_t>(data, std::endian::little);
        uint16_t big = sg::bytes::to_integral<uint16_t>(data, std::endian::big);
        CHECK(big == 1);
        CHECK(little == 256);
    }

    {
        uint8_t data[4] = {0x00, 0x01, 0x10, 0x01};
        uint32_t little = sg::bytes::to_integral<uint32_t>(data, std::endian::little);
        uint32_t big = sg::bytes::to_integral<uint32_t>(data, std::endian::big);
        CHECK(big == 69633);
        CHECK(little == 17826048);
    }

    CHECK_EQ(0x0110, sg::bytes::byteswap(uint16_t(0x1001)));
    CHECK_EQ(0x0110, sg::bytes::byteswap(int16_t(0x1001)));
    CHECK_EQ(0xEFBEADDE, sg::bytes::byteswap(uint32_t(0xDEADBEEFu)));
    CHECK_EQ(0xEFBEADDE, sg::bytes::byteswap(int32_t(0xDEADBEEFu)));
    CHECK_EQ(0xEFCDAB8967452301, sg::bytes::byteswap(uint64_t(0x0123456789ABCDEFull)));
    CHECK_EQ(0xEFCDAB8967452301, sg::bytes::byteswap(int64_t(0x0123456789ABCDEFull)));
}
