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

    /* checking constness */
    {
        const sg::data::vector_channel<int> vec{0,2,5};

        REQUIRE(vec.at(0)==0);
        REQUIRE(vec.at(1)==2);
        REQUIRE(vec.at(2)==5);

        REQUIRE(vec.front() == 0);
        REQUIRE(vec.back() == 5);

        REQUIRE(vec.begin() == vec.cbegin());
        REQUIRE(vec.end() == vec.cend());

        REQUIRE(vec.end() - vec.begin() == 3);
        REQUIRE(*(vec.begin() + 1) == 2);
        REQUIRE(*(vec.end() - 1) == 5);

        REQUIRE(vec.count() ==3);
        REQUIRE(vec.size_bytes()  == 3*sizeof(int));
    }

    // /* do move-only */
    // {
    //     sg::data::vector_channel<std::unique_ptr<int>> moveVec;
    //     auto ptr = std::make_unique<int>();
    //     moveVec.emplace_back(std::move(ptr));
    // }
}

TEST_CASE("sg::data: vector_channel: check from_bytes()", "[sg::data]") {
    sg::data::vector_channel<int> chA;
    int data[] = {1,2,3,4,5};
    chA.from_bytes(&data, sizeof(data));
    REQUIRE(chA.front() ==1);
    REQUIRE(chA.back() ==5);
}

TEST_CASE("sg::data: vector_channel: check operator==", "[sg::data]") {
    sg::data::vector_channel<int> chA;
    sg::data::vector_channel<int> chB;

    REQUIRE(chA != chB);
}

TEST_CASE("sg::data: vector_channel: check append(IContigiousChannel<InT>&)", "[sg::data]") {
    sg::data::vector_channel<int> chA {0,1,2,3};
    sg::data::vector_channel<int> chB {4,5,6,7};

    chA.append(chB);

    REQUIRE(chA.front()==0);
    REQUIRE(chA[4]==4);
    REQUIRE(chA.back()==7);
}

TEST_CASE("sg::data: vector_channel: check append(InputIt&&, InputIt&&)", "[sg::data]") {
    // Add raw bytes
    {
        sg::data::vector_channel<int> chA{0, 1, 2, 3};
        int data[] = {4, 5, 6, 7};
        chA.append(&data[0], &data[4]);

        REQUIRE(chA.front() == 0);
        REQUIRE(chA[4] == 4);
        REQUIRE(chA.back() == 7);
    }

    //Add another vector (const)
    {
        sg::data::vector_channel<int> chA{0, 1, 2, 3};
        const std::vector<int> data = {4, 5, 6, 7};
        chA.append(data);

        REQUIRE(chA.front() == 0);
        REQUIRE(chA[4] == 4);
        REQUIRE(chA.back() == 7);
    }

    //Add another vector (move)
    {
        sg::data::vector_channel<int> chA{0, 1, 2, 3};
        std::vector<int> data = {4, 5, 6, 7};
        chA.append(std::move(data));

        REQUIRE(chA.front() == 0);
        REQUIRE(chA[4] == 4);
        REQUIRE(chA.back() == 7);
    }
}
