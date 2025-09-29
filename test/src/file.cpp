#include <catch2/catch_test_macros.hpp>

#include <sg/file.h>

#include <iostream>
#include <fstream>

TEST_CASE("sg::file read(...)", "[sg::file]") {
    std::filesystem::path path = "test.txt";
    std::string content = "test string";

    SECTION("returning contents in a new buffer") {
        std::ofstream myfile;
        myfile.open(path, std::ios::out | std::ios::trunc);
        myfile << content;
        myfile.close();

        auto readback = sg::common::file::read(path);
        std::string redbackStr((char*)(&*readback.begin()), (char*)(&*readback.end()));
        REQUIRE(redbackStr==content);
    }

    SECTION("saving contents to existing buffer") {
        std::ofstream myfile;
        myfile.open (path, std::ios::out | std::ios::trunc);
        myfile << content;
        myfile.close();

        auto buffer = sg::make_shared_c_buffer<std::byte>(content.size());
        sg::common::file::read(path, buffer.get(), buffer.size());
        std::string redbackStr((char*)(&*buffer.begin()), (char*)(&*buffer.end()));

        REQUIRE(redbackStr==content);
    }

    SECTION("check opening in wrong mode throws exception") {
        auto buffer = sg::make_shared_c_buffer<std::byte>(content.size());
        REQUIRE_THROWS(sg::common::file::read(path, buffer.get(), buffer.size(), std::ios::out));
    }

}


TEST_CASE("sg::file void write(...)", "[sg::file]") {
    std::filesystem::path path = "test.txt";
    std::string content = "test string";

    sg::common::file::write(path, (const std::byte*)content.data(), content.size());

    auto readback = sg::common::file::read(path);
    std::string redbackStr((char*)(&*readback.begin()), (char*)(&*readback.end()));

    REQUIRE(redbackStr==content);

    SECTION("check opening in wrong mode throws exception") {
        REQUIRE_THROWS(sg::common::file::write(path, (const std::byte*)content.data(),
                                               content.size(), std::ios::in));
    }
}