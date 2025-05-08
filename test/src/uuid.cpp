#include <catch2/catch_test_macros.hpp>

#include <sg/uuid.h>

TEST_CASE("sg::uuid: check to_string()", "[sg::uuid]") {
    auto arr = std::array<uint8_t, 16>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    auto uuid = sg::uuids::uuid(arr);
    REQUIRE(uuid.to_sting() == "00010203-0405-0607-0809-0a0b0c0d0e0f");
}

TEST_CASE("sg::uuid: check ==operator()", "[sg::uuid]") {
    auto arr = std::array<uint8_t, 16>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    auto uuid = sg::uuids::uuid(arr);
    auto uuid2=sg::uuids::uuid(uuid);
    REQUIRE(uuid==uuid2);
    REQUIRE(uuid2.to_sting() == "00010203-0405-0607-0809-0a0b0c0d0e0f");
}
