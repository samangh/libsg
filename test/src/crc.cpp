#include <sg/crc.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("SG::common sg::checksum: check crc", "[sg::checksum]") {
    // see https://crccalc.com/

    std::string input = "123456789";

    {
        auto crc16 = sg::checksum::crc16(input.c_str(), input.length());
        auto crc32 = sg::checksum::crc32(input.c_str(), input.length());
        auto crc32c = sg::checksum::crc32c(input.c_str(), input.length());

        REQUIRE(crc16 == 0xBB3D);
        REQUIRE(crc32 == 0xCBF43926);
        REQUIRE(crc32c == 0xE3069283);
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
        auto remainder2 = sg::checksum::crc32c(input2.c_str(), input2.length(), ~remainder1);
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
        auto remainder2 = sg::checksum::crc32c(input2.c_str(), input2.length(), ~remainder1);
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
        auto remainder2 = sg::checksum::crc32c(input2.c_str(), input2.length(), ~remainder1);
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
        auto remainder2 = sg::checksum::crc32c(input2.c_str(), input2.length(), ~remainder1);
        REQUIRE(remainder2 == 0x9F754A85);
    }
}
