#include "sg/data/channel_vector.h"
#include <catch2/catch_test_macros.hpp>


TEST_CASE("sg::data check channel_vector<>(...) family", "[sg::data]") {
    auto name  = "chname";
    std::vector<std::string> hier = {"group0", "subgroup0"};

    {
        sg::data::vector_channel<int> vec;
        vec.name(name);
        vec.hierarchy(hier);

        vec.push_back(0);
        vec.push_back(2);
        vec.push_back(5);

        REQUIRE(vec[0]==0);
        REQUIRE(vec[1]==2);
        REQUIRE(vec[2]==5);

        REQUIRE(vec.at(0)==0);
        REQUIRE(vec.at(1)==2);
        REQUIRE(vec.at(2)==5);

        REQUIRE(vec.front() == 0);
        REQUIRE(vec.back() == 5);

        REQUIRE(vec.end() - vec.begin() == 3);
        REQUIRE(*(vec.begin() + 1) == 2);
        REQUIRE(*(vec.end() - 1) == 5);

        REQUIRE(vec.name() == name);
        REQUIRE(vec.hierarchy() == hier);

        REQUIRE(vec.count() ==3);
        REQUIRE(vec.size_bytes()  == 3*sizeof(int));

    }

    /* do move-only */
    {
        sg::data::vector_channel<std::unique_ptr<int>> moveVec;
        auto ptr = std::make_unique<int>();
        moveVec.emplace_back(std::move(ptr));
    }
}
