#include <catch2/catch_test_macros.hpp>

#include <sg/enumeration.h>

enum class test_enum :int{
    E1 =1 ,
    E2 =2,
    E4 =4
};

TEST_CASE("sg::common::enumeration check operator|(...)", "[sg::common::enumeration]") {
    using namespace sg::enumeration;
    test_enum b = test_enum::E1 | test_enum::E2;
    REQUIRE(static_cast<int>(b) ==3);
}

TEST_CASE("sg::common::enumeration check operator|=(...)", "[sg::common::enumeration]") {
    using namespace sg::enumeration;
    test_enum b = test_enum::E1;
    b |=  test_enum::E2;
    REQUIRE(static_cast<int>(b) == 3);
}

TEST_CASE("sg::common::enumeration check operator&(...)", "[sg::common::enumeration]") {
    using namespace sg::enumeration;
    test_enum b = test_enum::E1 | test_enum::E2;
    REQUIRE((b & test_enum::E1)  == test_enum::E1);
    REQUIRE(static_cast<int>(b & test_enum::E4)==0);
}

TEST_CASE("sg::common::enumeration check contains(...)", "[sg::common::enumeration]") {
    using namespace sg::enumeration;
    test_enum a = test_enum::E1 | test_enum::E2;
    REQUIRE(contains(a, test_enum::E1));
    REQUIRE(contains(a, test_enum::E2));
    REQUIRE(!contains(a, test_enum::E4));
}

TEST_CASE("sg::common::enumeration check underlying_value(...)", "[sg::common::enumeration]") {
    using namespace sg::enumeration;
    test_enum a = test_enum::E1 | test_enum::E2;
    REQUIRE(underlying_value(test_enum::E1)==1);
    REQUIRE(underlying_value(test_enum::E2)==2);
    REQUIRE(underlying_value(a)==3);
}

TEST_CASE("sg::common::enumeration check from_underlying_value(...)", "[sg::common::enumeration]") {
    using namespace sg::enumeration;

    REQUIRE(from_underlying_value<test_enum>(1)==test_enum::E1);
    REQUIRE(from_underlying_value<test_enum>(2)==test_enum::E2);

    auto b =from_underlying_value<test_enum>(3);
    REQUIRE(contains(b, test_enum::E1));
    REQUIRE(contains(b, test_enum::E2));
    REQUIRE(!contains(b, test_enum::E4));
}
