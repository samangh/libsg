#include <catch2/catch_test_macros.hpp>

#include <sg/locale.h>

TEST_CASE("sg::locale set_ctype_globally(...)", "[sg::locale]") {
    // Set local and check
    REQUIRE_NOTHROW(sg::locale::set_ctype_globally("C"));
    REQUIRE(std::strcmp(std::setlocale(LC_CTYPE, nullptr), "C") ==0);

    REQUIRE_THROWS(sg::locale::set_ctype_globally("dsadsad"));
}

TEST_CASE("sg::locale enable_utf8_encoding_globally(...)", "[sg::locale]") {
    REQUIRE_NOTHROW(sg::locale::enable_utf8_encoding_globally());
    // Go back to default LC_CTYPE, as to not affect the rest of the program
    sg::locale::set_ctype_globally("C");
}
