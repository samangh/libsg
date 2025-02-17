#include <catch2/catch_test_macros.hpp>

#include <sg/vector.h>

TEST_CASE("sg::vector: check append for intrinsic types", "[sg::vector]") {
    //lvalue
    {
        std::vector<int> first = {1, 2, 3};
        std::vector<int> second{4, 5, 6};

        sg::vector::append(first, second);
        REQUIRE(first == std::vector<int>{1, 2, 3, 4, 5, 6});
    }

    //rvalue
    {
        std::vector<int> first = {1, 2, 3};
        std::vector<int> second{4, 5, 6};

        sg::vector::append(first, std::move(second));
        REQUIRE(first == std::vector<int>{1, 2, 3, 4, 5, 6});
    }
}


size_t moved=0;
size_t copy_or_constructed=0;

TEST_CASE("sg::vector: check copy-only append for complex types", "[sg::vector]") {

    class obj {
      public:
        obj() {
            copy_or_constructed++;
        };
        obj(const obj&) {
            copy_or_constructed++;
        };
        obj& operator=(const obj& ) {
            copy_or_constructed++;
            return *this;
        };

        // Move constrcutor
        obj(obj&&) {
            moved++;
        };
        obj& operator=(obj&&) {
            moved++;
            return *this;
        };
    };

    //copy
    {
        std::vector<obj> first(2);
        std::vector<obj> second(1);
        first.reserve(3); //to prevent vector resizing

        copy_or_constructed=0;
        moved=0;

        sg::vector::append(first, second);

        REQUIRE(first.size()==3);
        REQUIRE(moved==0);
        REQUIRE(copy_or_constructed==1);
    }

    //move
    {
        std::vector<obj> first(2);
        std::vector<obj> second(1);
        first.reserve(3); //to prevent vector resizing

        copy_or_constructed=0;
        moved=0;

        sg::vector::append(first, std::move(second));

        REQUIRE(first.size()==3);
        REQUIRE(moved==1);
        REQUIRE(copy_or_constructed==0);
    }
}
