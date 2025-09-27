#include <sg/string.h>

#include <catch2/catch_test_macros.hpp>
#include <iostream>

TEST_CASE("sg::string: to_string()", "[sg::string]") {
    sg::common::set_global_utf8_codepage();

    // Test values from:
    //   https://en.cppreference.com/w/c/string/multibyte/mbsrtowcs.html
    //   https://en.cppreference.com/w/cpp/string/basic_string/size.html

    std::string input1 = "z\u00df\u6c34\U0001f34c";                          // zß水🍌
    std::string input2 = "\u30cf\u30ed\u30fc\u30fb\u30ef\u30fc\u30eb\u30c9"; // ハロー・ワールドc

    // const char*
    REQUIRE(sg::common::to_wstring(input1.c_str()).size() == 4);
    REQUIRE(sg::common::to_wstring(input2.c_str()).size() == 8);

    // std::sting
    REQUIRE(sg::common::to_wstring(input1).size() == 4);
    REQUIRE(sg::common::to_wstring(input2).size() == 8);
}
