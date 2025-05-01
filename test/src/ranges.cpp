#include <catch2/catch_test_macros.hpp>

#include <sg/ranges.h>

TEST_CASE("sg::common::ranges erase_value(...)", "[sg::common::ranges]") {
    std::vector<int> vec{0,1,2,2,3,3,3};

    sg::ranges::erase_value(vec, 1);
    REQUIRE(vec == std::vector<int> {0,2,2,3,3,3});

    sg::ranges::erase_value(vec, 2);
    REQUIRE(vec == std::vector<int> {0,3,3,3});

    sg::ranges::erase_value(vec, 100);
    REQUIRE(vec == std::vector<int> {0,3,3,3});}

TEST_CASE("sg::common::ranges erase_value_if(...)", "[sg::common::ranges]") {
    std::vector<int> vec{0,1,2,2,3,3,3};

    sg::ranges::erase_value_if(vec, [](const auto& v){return v==1;});
    REQUIRE(vec == std::vector<int> {0,2,2,3,3,3});

    sg::ranges::erase_value_if(vec, [](const auto& v){return v==2;});
    REQUIRE(vec == std::vector<int> {0,3,3,3});

    sg::ranges::erase_value_if(vec, [](const auto& v){return v==100;});
    REQUIRE(vec == std::vector<int> {0,3,3,3});
}


TEST_CASE("sg::vector: check append for intrinsic types", "[sg::ranges]") {
    //lvalue
    {
        std::vector<int> first = {1, 2, 3};
        std::vector<int> second{4, 5, 6};

        sg::ranges::append(first, second);
        REQUIRE(first == std::vector<int>{1, 2, 3, 4, 5, 6});
    }

    //rvalue
    {
        std::vector<int> first = {1, 2, 3};
        std::vector<int> second{4, 5, 6};

        sg::ranges::append(first, std::move(second));
        REQUIRE(first == std::vector<int>{1, 2, 3, 4, 5, 6});
    }
}

template <typename T>
using vec = std::vector<T>;

TEST_CASE("sg::vector: nested range flatenning", "[sg::ranges]") {
    auto top = vec<vec<vec<int>>>();
    auto mid1 = vec<vec<int>>();
    auto mid2 = vec<vec<int>>();
    auto bot = vec<int>{1, 2, 3};

    mid1.push_back(bot);
    mid1.push_back(bot);

    mid2.push_back(bot);
    mid2.push_back(bot);

    top.push_back(mid1);
    top.push_back(mid2);

    //flatenning down to base init
    {
        auto flattened = sg::ranges::flatten<int>(top);
        auto correc_resut = vec<int>{1,2,3, 1,2,3, 1,2,3, 1,2,3};

        REQUIRE(sg::ranges::flatenned_size<int>(top) == 12);
        REQUIRE(flattened == correc_resut);
    }

    //flatenning down to vec<int>
    {
        auto flattened = sg::ranges::flatten<vec<int>>(top);
        auto correc_resut = vec<vec<int>>{{1,2,3},{1,2,3},{1,2,3},{1,2,3} };

        REQUIRE(sg::ranges::flatenned_size<vec<int>>(top) == 4);
        REQUIRE(flattened == correc_resut);
    }

    //flatenning down to base init with custom no-init allocator
    {
        auto flattened = sg::ranges::flatten<int, sg::ranges::allocator_no_init<int>>(top);
        auto correc_resut = sg::ranges::vector_no_init<int>{1,2,3, 1,2,3, 1,2,3, 1,2,3};

        REQUIRE(sg::ranges::flatenned_size<int>(top) == 12);
        REQUIRE(flattened == correc_resut);
    }

    //flatenning down to base init with flatten_no_init
    {
        auto flattened = sg::ranges::flatten_no_init<int>(top);
        auto correc_resut = sg::ranges::vector_no_init<int>{1,2,3, 1,2,3, 1,2,3, 1,2,3};

        REQUIRE(sg::ranges::flatenned_size<int>(top) == 12);
        REQUIRE(flattened == correc_resut);
    }
}

size_t moved=0;
size_t copy_or_constructed=0;

TEST_CASE("sg::vector: check copy-only append for complex types", "[sg::ranges]") {

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

        sg::ranges::append(first, second);

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

        sg::ranges::append(first, std::move(second));

        REQUIRE(first.size()==3);
        REQUIRE(moved==1);
        REQUIRE(copy_or_constructed==0);
    }
}
