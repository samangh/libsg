#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "sg/meta.h"

#include <string>
#include <vector>

namespace {

struct a_struct {};

} // namespace

TEST_CASE("sg::meta::type_name: check fundamental types", "[sg::meta]") {
    REQUIRE(sg::meta::type_name<int>() == "int");
    REQUIRE(sg::meta::type_name<double>() == "double");

    const int i{};
    REQUIRE(sg::meta::type_name<decltype(i)>() == "const int");
}

TEST_CASE("sg::meta::type_name: check the name is usable at compile time", "[sg::meta]") {
    static_assert(sg::meta::type_name<int>() == "int");
    static_assert(!sg::meta::type_name<a_struct>().empty());
}

TEST_CASE("sg::meta::type_name: check class types", "[sg::meta]") {
    /* MSVC spells these differently (e.g. "class std::vector<int,...>"), so only check that the
     * interesting part is in there. */
    REQUIRE_THAT(std::string(sg::meta::type_name<a_struct>()),
                 Catch::Matchers::ContainsSubstring("a_struct"));
    REQUIRE_THAT(std::string(sg::meta::type_name<std::vector<int>>()),
                 Catch::Matchers::ContainsSubstring("vector<int"));
}

TEST_CASE("sg::meta::type_name: check nothing is left of the compiler's signature", "[sg::meta]") {
    REQUIRE_THAT(std::string(sg::meta::type_name<std::vector<int>>()),
                 !Catch::Matchers::ContainsSubstring("type_name"));
    REQUIRE_THAT(std::string(sg::meta::type_name<std::vector<int>>()),
                 !Catch::Matchers::ContainsSubstring("Error"));
}
