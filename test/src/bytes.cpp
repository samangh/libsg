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
}

