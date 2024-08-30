#include <cstring>
#include <sg/file_writer.h>
#include <sg/file_writer_uv.h>

#include <doctest/doctest.h>
#include <nanobench.h>

#include <fstream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <cstring>
#include <filesystem>
#include <numeric>

static std::string read_file(std::string path) {
    std::ifstream t(path);
    return std::string((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
}

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

TEST_CASE("SG::common sg::file_writer: check writer flushes correctly") {
    std::string text = "TEST";
    std::string path = "test.txt";

    {
        sg::file_writer writer;
        writer.start(path, nullptr, nullptr, nullptr);

        auto a = sg::make_shared_c_buffer<std::byte>(text.size());
        std::memcpy(a.get(), text.data(), text.size() * sizeof(char));
        writer.write_aync(std::move(a));

        writer.stop();

        CHECK(read_file(path) == text);
    }

    {
        {
            sg::file_writer writer;
            writer.start(path, nullptr, nullptr, nullptr);

            auto a = sg::make_shared_c_buffer<std::byte>(text.size());
            std::memcpy(a.get(), text.data(), text.size() * sizeof(char));
            writer.write_aync(std::move(a));
        }

        std::ifstream t(path);
        std::string str((std::istreambuf_iterator<char>(t)),
                        std::istreambuf_iterator<char>());

        CHECK(read_file(path) == text);
    }
}

TEST_CASE("SG::common sg::file_writer: check write_async(...) variations work") {
    std::string text = "TEST";
    std::string path = "test.txt";

    {
        sg::file_writer writer;
        writer.start(path, nullptr, nullptr, nullptr);

        SUBCASE("crate shared_buffer") {
            auto a = sg::make_shared_c_buffer<std::byte>(text.size());
            std::memcpy(a.get(), text.data(), text.size() * sizeof(char));
            writer.write_aync(std::move(a));
        }

        SUBCASE("pass pointer") {
            writer.write_aync(text.data(), text.size());
        }

        SUBCASE("pass string_view") {
            writer.write_aync(text);
        }

        writer.stop();
    }

    CHECK(read_file(path) == text);
}

TEST_CASE("SG::common sg::file_writer: check that you can't start twice") {
    std::string text = "TEST";
    std::string path = "test.txt";

    sg::file_writer writer;
    writer.start(path, nullptr, nullptr, nullptr);
    CHECK_THROWS(writer.start(path, nullptr, nullptr, nullptr));
}

TEST_CASE("SG::common sg::file_writer: check bytes_transferred()") {
    std::string text = "TEST";
    std::string path = "test.txt";

    sg::file_writer writer;
    writer.start(path, nullptr, nullptr, nullptr);
    CHECK_EQ(writer.bytes_transferred(), 0);

    writer.write_aync(text);
    writer.stop();
    CHECK_EQ(writer.bytes_transferred(), 4);
}

TEST_CASE("SG::common sg::file_writer: test large sequental writes") {
    std::string path = "sequ-write";
    std::vector<std::vector<uint64_t>> vec_data = random_data(1024, 2048); //2 MiB

    {
        sg::file_writer writer;
        writer.start(path, nullptr, nullptr, nullptr);
        for (const auto& buf : vec_data) {
            auto data_buf = sg::make_shared_c_buffer<std::byte>(buf.size() * sizeof(uint64_t));
            std::memcpy(data_buf.get(), &buf[0], buf.size() * sizeof(uint64_t));
            writer.write_aync(std::move(data_buf));
        }
        writer.stop();
    }

    CHECK_EQ(std::filesystem::file_size(path), 1024*2048);
}

#ifdef BENCHMARK
TEST_CASE("SG::common file_writer and file_writer_uv performance" ){
    std::string path = "benchmark-buffer";
    std::vector<std::vector<uint64_t>> vec_data;

    vec_data =   random_data(500, 1500);
    SUBCASE("sg::file_writer(..)") {
        ankerl::nanobench::Bench().minEpochIterations(100).run("sg::file_writer(..)", [&]() {
            sg::file_writer writer;
            writer.start(path, nullptr, nullptr, nullptr);
            for (const auto& buf : vec_data) {
                auto data_buf = sg::make_shared_c_buffer<std::byte>(buf.size() * sizeof(uint64_t));
                std::memcpy(data_buf.get(), &buf[0], buf.size() * sizeof(uint64_t));
                writer.write_aync(std::move(data_buf));
            }
            writer.stop();
        });
    }

    vec_data =   random_data(500, 1500);
    SUBCASE("sg::file_writer_uv(..)") {
        ankerl::nanobench::Bench().minEpochIterations(100).run("sg::file_writer_uv(..)", [&]() {
            sg::file_writer_uv writer;
            writer.start(path, nullptr, nullptr, 100);
            for (const auto& buf : vec_data) {
                auto data_buf = sg::make_shared_c_buffer<std::byte>(buf.size() * sizeof(uint64_t));
                std::memcpy(data_buf.get(), &buf[0], buf.size() * sizeof(uint64_t));
                writer.write_async(std::move(data_buf));
            }
            writer.stop();
        });
    }

}
#endif
