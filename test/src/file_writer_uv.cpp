#ifdef _MSC_VER
    #pragma warning(disable : 4996)
#else
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <cstring>
#include <sg/file_writer_uv.h>

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <random>
#include <algorithm>
#include <numeric>

static std::vector<std::vector<uint64_t>> random_data(std::size_t count, std::size_t bytes)
{
    std::vector<std::vector<uint64_t>> vec_data;
    for (size_t i=0; i< count; ++i)
    {
        std::vector<uint64_t> data(bytes / sizeof(uint64_t));
        std::iota(data.begin(), data.end(), 0);
        std::shuffle(data.begin(), data.end(), std::mt19937{std::random_device{}()});
        vec_data.emplace_back(std::move(data));
    }
    return vec_data;
}

TEST_CASE("SG::common sg::filewiter_uv: check async() write following by stop()") {
    std::string text = "TEST";
    std::string path = "test.txt";

    /* using .stop() */
    {
        sg::file_writer_uv writer;
        writer.start(path, nullptr, nullptr, 200);
        writer.write_async(text);
        writer.stop();

        std::ifstream t(path);
        std::string str((std::istreambuf_iterator<char>(t)),
                        std::istreambuf_iterator<char>());

        CHECK(str == text);
    }

    /* depending on constructor to flush */
    {
        {
            sg::file_writer_uv writer;
            writer.start(path, nullptr, nullptr, 200);
            writer.write_async(text);
        }

        std::ifstream t(path);
        std::string str((std::istreambuf_iterator<char>(t)),
                        std::istreambuf_iterator<char>());

        CHECK(str == text);
    }
}

TEST_CASE("SG::common sg::file_writer_uv: test large sequental writes") {
    std::string path = "sequ-write";
    std::vector<std::vector<uint64_t>> vec_data = random_data(1024, 2048); //2 MiB

    {
        sg::file_writer_uv writer;
        writer.start(path, nullptr, nullptr, 200);
        for (const auto& buf : vec_data) {
            auto data_buf = sg::make_shared_c_buffer<std::byte>(buf.size() * sizeof(uint64_t));
            std::memcpy(data_buf.get(), &buf[0], buf.size() * sizeof(uint64_t));
            writer.write_async(std::move(data_buf));
        }
        writer.stop();
    }

    CHECK(std::filesystem::file_size(path) == 1024*2048);
}

TEST_CASE("SG::common sg::file_writer_uv: check that you can't start twice") {
    std::string text = "TEST";
    std::string path = "test.txt";

    sg::file_writer_uv writer;
    writer.start(path, nullptr, nullptr, 200);
    CHECK_THROWS(writer.start(path, nullptr, nullptr, 200));
}
