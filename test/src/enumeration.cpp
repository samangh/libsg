#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include <sg/enumeration.h>

enum class test_enum :int{
    E1 =1 ,
    E2 =2,
    E4 =4
};

enum class test_enum2 :int{
    E1 =1 ,
    E2 =2,
    E4 =4
};

enum test_c_enum {
    E1 =1,
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

TEMPLATE_TEST_CASE("sg::common::enumeration check contains(...)", "[sg::common::enumeration]",
                   test_enum, test_c_enum) {
    using namespace sg::enumeration;
    TestType a = TestType::E1 | TestType::E2;
    REQUIRE(contains(a, TestType::E1));
    REQUIRE(contains(a, TestType::E2));
    REQUIRE(!contains(a, TestType::E4));
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

void populate_enum_value_map(std::map<test_enum, std::string>& map) {
    map = std::map<test_enum, std::string>{
        {test_enum::E1, "E1"},
        {test_enum::E2, "E2"},
        {test_enum::E4, "E4"},
    };
}

void populate_enum_value_map(std::map<test_enum2, std::string>& map) {
    map = std::map<test_enum2, std::string>{
        {test_enum2::E1, "E1"},
        {test_enum2::E2, "E1"},
        {test_enum2::E4, "E4"},
    };
}

TEST_CASE("sg::common::enumeration check enum_to_val(...)", "[sg::common::enumeration]") {
    REQUIRE(sg::enumeration::enum_to_val<std::string>(test_enum::E1) == "E1");
    REQUIRE(sg::enumeration::enum_to_val<std::string>(test_enum::E2) == "E2");
    REQUIRE(sg::enumeration::enum_to_val<std::string>(test_enum::E4) == "E4");
}

TEST_CASE("sg::common::enumeration check enum_from_val(...)", "[sg::common::enumeration]") {
    REQUIRE(sg::enumeration::enum_from_val<test_enum>(std::string("E1")) == test_enum::E1);
    REQUIRE(sg::enumeration::enum_from_val<test_enum>(std::string("E2")) == test_enum::E2);
    REQUIRE(sg::enumeration::enum_from_val<test_enum>(std::string("E4")) == test_enum::E4);

    REQUIRE_THROWS(sg::enumeration::enum_to_val<std::string>(test_enum2::E4));
    REQUIRE_THROWS(sg::enumeration::enum_from_val<test_enum2>(std::string("E1")));
}
