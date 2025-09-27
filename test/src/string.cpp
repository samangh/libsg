#include <sg/string.h>

#include <catch2/catch_test_macros.hpp>
#include <iostream>

TEST_CASE("sg::string: to_wstring()", "[sg::string]") {
    sg::common::set_global_utf8_codepage();

    // Test values from:
    //   https://en.cppreference.com/w/c/string/multibyte/mbsrtowcs.html
    //   https://en.cppreference.com/w/cpp/string/basic_string/size.html

    std::string input1 = "zß水🍌";     // z\u00df\u6c34\U0001f34c";
    std::string input2 = "ハロー・ワールド"; // \u30cf\u30ed\u30fc\u30fb\u30ef\u30fc\u30eb\u30c9";

    size_t input1_wlen = 4;
    size_t input2_wlen = 8;

    /* The Unicode "banana" emoji above needs 4-bytes. So on systems where wchar_t is 2 bytes,
     * an extra wchar_t is need to represent it. */
    if constexpr (sizeof(wchar_t)==2) {
        input1_wlen++;
    }

    // const char*
    REQUIRE(sg::common::to_wstring(input1.c_str()).size() == input1_wlen);
    REQUIRE(sg::common::to_wstring(input2.c_str()).size() == input2_wlen);

    // std::string
    REQUIRE(sg::common::to_wstring(input1).size() == input1_wlen);
    REQUIRE(sg::common::to_wstring(input2).size() == input2_wlen);
}
