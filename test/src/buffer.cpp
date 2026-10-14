#include <sg/buffer.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

TEST_CASE("sg::unique_buffer: check construction and reset(...)", "[sg::buffer]") {
    auto buff = sg::unique_buffer<int>(new int[4]{1, 2, 3, 4}, 4);
    REQUIRE(buff.size() == 4);
    REQUIRE(buff.get()[3] == 4);

    buff.reset(new int[2]{5, 6}, 2);
    REQUIRE(buff.size() == 2);
    REQUIRE(buff.get()[1] == 6);

    buff.reset();
    REQUIRE(buff.size() == 0);
    REQUIRE(buff.get() == nullptr);
}

TEST_CASE("sg::shared_buffer: check construction and reset(...)", "[sg::buffer]") {
    auto buff = sg::shared_buffer<int>(new int[4]{1, 2, 3, 4}, 4);
    REQUIRE(buff.size() == 4);
    REQUIRE(buff.get()[3] == 4);

    buff.reset(new int[2]{5, 6}, 2);
    REQUIRE(buff.size() == 2);
    REQUIRE(buff.get()[1] == 6);

    buff.reset();
    REQUIRE(buff.size() == 0);
    REQUIRE(buff.get() == nullptr);
}

TEST_CASE("sg::unique_c_buffer: check reset(...) keeps the custom deleter", "[sg::buffer]") {
    auto buff = sg::make_unique_c_buffer<char>(16);
    REQUIRE(buff.size() == 16);

    /* the replacement pointer must be released with free(), not delete[] */
    buff.reset(static_cast<char *>(std::malloc(32)), 32);
    REQUIRE(buff.size() == 32);
}

TEST_CASE("sg::shared_c_buffer: check reset(...) keeps the custom deleter", "[sg::buffer]") {
    auto buff = sg::make_shared_c_buffer<char>(16);
    REQUIRE(buff.size() == 16);

    /* the replacement pointer must be released with free(), not delete[] */
    buff.reset(static_cast<char *>(std::malloc(32)), 32);
    REQUIRE(buff.size() == 32);
}

TEST_CASE("sg::buffer: check swap(...)", "[sg::buffer]") {
    auto a = sg::unique_buffer<int>(new int[1]{1}, 1);
    auto b = sg::unique_buffer<int>(new int[2]{2, 3}, 2);

    a.swap(b);

    REQUIRE(a.size() == 2);
    REQUIRE(a.get()[0] == 2);
    REQUIRE(b.size() == 1);
    REQUIRE(b.get()[0] == 1);
}

TEST_CASE("sg::opaque_buffer: check make_*_opaque_buffer(...)", "[sg::buffer]") {
    auto uniq = sg::make_unique_opaque_buffer<int>(4);
    REQUIRE(uniq.size() == 4);
    uniq.get()[0] = 1;
    REQUIRE(uniq.get()[0] == 1);

    auto shared = sg::make_shared_opaque_buffer<int>(4);
    REQUIRE(shared.size() == 4);
    shared.get()[0] = 1;
    REQUIRE(shared.get()[0] == 1);
}
