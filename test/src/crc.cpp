#include <sg/crc.h>
#include <sg/random.h>

#include <catch2/catch_all.hpp>

#include <string>

TEST_CASE("sg::checksum: check crc", "[sg::checksum]") {
    // see https://crccalc.com/

    /* long input, as lots of bytes (>144 for intel, >112 for AMD) are needed to kick-in the harware CR32*/
    std::string input = "123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678123456781234567812345678";
    {
        auto crc16 = sg::checksum::crc16(input.c_str(), input.length());
        auto crc32 = sg::checksum::crc32(input.c_str(), input.length());
        auto crc32c = sg::checksum::crc32c(input.c_str(), input.length());

        REQUIRE(crc16 == 0xFBC7);
        REQUIRE(crc32 == 0x385631DF);
        REQUIRE(crc32c == 0x2891D639);
    }

    //partial crc32 - 1 byte
    {
        std::string input1 = "1";
        auto remainder1 = sg::checksum::crc32c(input1.c_str(), input1.length());
        REQUIRE(remainder1 == 0x90F599E3);
    }

   //partial crc32 - 2 byte
    {
        std::string input1 = "12";
        auto remainder1 = sg::checksum::crc32c(input1.c_str(), input1.length());
        REQUIRE(remainder1 == 0x7355C460);
    }

    //partial crc32 - 4 byte
    {
        std::string input1 = "1234";
        auto remainder1 = sg::checksum::crc32c(input1.c_str(), input1.length());
        REQUIRE(remainder1 == 0xF63AF4EE);
    }

    //partial crc32 - 8 byte
    {
        std::string input1 = "12345678";
        auto remainder1 = sg::checksum::crc32c(input1.c_str(), input1.length());
        REQUIRE(remainder1 == 0x6087809A);
    }

    //partial crc32 - 16 byte
    {
        std::string input1 = "1234567812345678";
        auto remainder1 = sg::checksum::crc32c(input1.c_str(), input1.length());
        REQUIRE(remainder1 == 0x43657BA8);
    }


    //partial crc32 - 2 + 1 byte
    {
        std::string input = "121";
        auto remainder = sg::checksum::crc32c(input.c_str(), input.length());
        REQUIRE(remainder == 0xF1405F45);

        std::string input1 = "12";
        std::string input2 = "1";

        auto remainder1 = sg::checksum::crc32c(input1.c_str(), input1.length());
        auto remainder2 = sg::checksum::crc32c(input2.c_str(), input2.length(), remainder1);
        REQUIRE(remainder2 == 0xF1405F45);
    }

    //partial crc32 - 4 + 2 byte
    {
        std::string input = "123412";
        auto remainder = sg::checksum::crc32c(input.c_str(), input.length());
        REQUIRE(remainder == 0xC8258745);

        std::string input1 = "1234";
        std::string input2 = "12";

        auto remainder1 = sg::checksum::crc32c(input1.c_str(), input1.length());
        auto remainder2 = sg::checksum::crc32c(input2.c_str(), input2.length(), remainder1);
        REQUIRE(remainder2 == 0xC8258745);
    }

    //partial crc32 - 8 + 4 byte
    {
        std::string input = "123456781234";
        auto remainder = sg::checksum::crc32c(input.c_str(), input.length());
        REQUIRE(remainder == 0x6F921B16);

        std::string input1 = "12345678";
        std::string input2 = "1234";

        auto remainder1 = sg::checksum::crc32c(input1.c_str(), input1.length());
        auto remainder2 = sg::checksum::crc32c(input2.c_str(), input2.length(), remainder1);
        REQUIRE(remainder2 == 0x6F921B16);
    }

    //partial crc32 - 16 + 8 byte
    {
        std::string input = "123456781234567812345678";
        auto remainder = sg::checksum::crc32c(input.c_str(), input.length());
        REQUIRE(remainder == 0x9F754A85);

        std::string input1 = "1234567812345678";
        std::string input2 = "12345678";

        auto remainder1 = sg::checksum::crc32c(input1.c_str(), input1.length());
        auto remainder2 = sg::checksum::crc32c(input2.c_str(), input2.length(), remainder1);
        REQUIRE(remainder2 == 0x9F754A85);
    }

    //whole chain: 16+8+4+2+1
    {
        std::string input = "1234567812345678123456781234121";
        REQUIRE(sg::checksum::crc32c(input.c_str(), input.length()) == 0x46BB7D06);
    }
}

TEST_CASE("sg::checksum: check crc performnce", "[sg::checksum]" ){
    BENCHMARK_ADVANCED("crc32c(...), 1 MB input")(Catch::Benchmark::Chronometer meter) {
        auto dat = sg::random::generate<uint8_t>(1000*1000);
        meter.measure([&dat] {
            return sg::checksum::crc32c(dat.data(), dat.size());
        });
    };
}
