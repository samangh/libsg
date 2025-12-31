#include <catch2/catch_test_macros.hpp>

#include <sg/uuid.h>

TEST_CASE("sg::uuid: check to_string()", "[sg::uuid]") {
    auto arr = std::array<uint8_t, 16>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    auto uuid = sg::uuids::uuid(arr);
    REQUIRE(uuid.to_string() == "00010203-0405-0607-0809-0a0b0c0d0e0f");
}

TEST_CASE("sg::uuid: check operator<<(...)", "[sg::uuid]") {
    auto arr = std::array<uint8_t, 16>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    auto uuid = sg::uuids::uuid(arr);
    std::stringstream ss;
    ss << uuid;
    REQUIRE(ss.str() == "00010203-0405-0607-0809-0a0b0c0d0e0f");
}

TEST_CASE("sg::uuid: check operator==()", "[sg::uuid]") {
    constexpr auto arr = std::array<uint8_t, 16>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    const auto uuid  = sg::uuids::uuid(arr);

    REQUIRE(uuid==sg::uuids::uuid(arr));
    REQUIRE(uuid==sg::uuids::uuid(uuid));
    REQUIRE(uuid != sg::uuids::uuid(std::array<uint8_t, 16>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,16}));
}

TEST_CASE("sg::uuid: check operator<=>", "[sg::uuid]") {
    auto uuid = sg::uuids::uuid(std::array<uint8_t, 16>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15});
    auto uuid2 = sg::uuids::uuid(std::array<uint8_t, 16>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,16});
    auto uuid3 = sg::uuids::uuid(std::array<uint8_t, 16>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15});
    REQUIRE((uuid <=> uuid2) == std::strong_ordering::less );
    REQUIRE((uuid2 <=> uuid) == std::strong_ordering::greater );
    REQUIRE((uuid <=> uuid3) == std::strong_ordering::equal );
}

TEST_CASE("sg::uuid: check swap(...)", "[sg::uuid]") {
    using std::swap;

    const auto orig = sg::uuids::uuid(std::array<uint8_t, 16>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15});
    const auto orig2 = sg::uuids::uuid(std::array<uint8_t, 16>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,16});

    auto test = orig;
    auto test2 = orig2;

    REQUIRE_NOTHROW(swap(test, test2));

    REQUIRE(test==orig2);
    REQUIRE(test2==orig);
}