#include <doctest/doctest.h>

auto multiply(int a, int b) { return a * b; }

TEST_CASE("Multiply using global function") {
	auto a =multiply(2,3);
    CHECK_EQ(a, 6);
}
