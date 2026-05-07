#include "sg/callback.h"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <type_traits>

CREATE_CALLBACK(cb1_t, int(int, int))
CREATE_CALLBACK(cb2_t, int(int, int))

static_assert(!std::is_same_v<cb1_t, cb2_t>,
              "CREATE_CALLBACK must produce distinct types per name");

TEST_CASE("sg:: check CREATE_CALLBACK(...) macro", "[sg::callback]") {
    cb1_t cb1 = [](int a, int b){ return a+b; };
    cb2_t cb2 = [](int a, int b){ return a+b; };
    cb1_t cb_empty = nullptr;

    REQUIRE(cb1.invoke(1,2)==3);
    REQUIRE(cb2.invoke(3,4)==7);
    REQUIRE_THROWS(cb_empty.invoke(1,2));
}