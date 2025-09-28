#include <catch2/catch_test_macros.hpp>

#include <sg/file.h>

#include <iostream>
#include <fstream>

TEST_CASE("sg::file get_contents(const std::filesystem::path&)", "[sg::file]") {
    std::filesystem::path path = "test.txt";
    std::string content = "test string";

    std::ofstream myfile;
    myfile.open (path, std::ios::out | std::ios::trunc);
    myfile << content;
    myfile.close();

    auto readback = sg::common::file::get_contents(path);
    std::string redbackStr((char*)(&*readback.begin()), (char*)(&*readback.end()));
    REQUIRE(redbackStr==content);
}

TEST_CASE("sg::file get_contents(const std::filesystem::path&, std::byte*, size_t)", "[sg::file]") {
    std::filesystem::path path = "test.txt";
    std::string content = "test string";

    std::ofstream myfile;
    myfile.open (path, std::ios::out | std::ios::trunc);
    myfile << content;
    myfile.close();

    auto buffer = sg::make_shared_c_buffer<std::byte>(content.size());
    sg::common::file::get_contents(path, buffer.get(), buffer.size());
    std::string redbackStr((char*)(&*buffer.begin()), (char*)(&*buffer.end()));

    REQUIRE(redbackStr==content);
}