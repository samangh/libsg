#ifdef _MSC_VER
    #pragma warning(disable : 4996)
#else
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <cstring>
#include <sg/file_writer.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_all.hpp>

#include <fstream>
#include <numeric>
#include <random>
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
        writer.write_async(std::move(a));

        writer.stop();

        CHECK(read_file(path) == text);
    }

    {
        {
            sg::file_writer writer;
            writer.start(path, nullptr, nullptr, nullptr);

            auto a = sg::make_shared_c_buffer<std::byte>(text.size());
            std::memcpy(a.get(), text.data(), text.size() * sizeof(char));
            writer.write_async(std::move(a));
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

        SECTION("crate shared_buffer") {
            auto a = sg::make_shared_c_buffer<std::byte>(text.size());
            std::memcpy(a.get(), text.data(), text.size() * sizeof(char));
            writer.write_async(std::move(a));
        }

        SECTION("pass pointer") {
            writer.write_async(text.data(), text.size());
        }

        SECTION("pass string_view") {
            writer.write_async(text);
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
    CHECK(writer.bytes_transferred()== 0);

    writer.write_async(text);
    writer.stop();
    CHECK(writer.bytes_transferred()== 4);
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
            writer.write_async(std::move(data_buf));
        }
        writer.stop();
    }

    CHECK(std::filesystem::file_size(path)== 1024*2048);
}

TEST_CASE("SG::common benchmark file_writer", "[sg::file_writer]" ){
    std::string path = "benchmark-buffer";

    BENCHMARK_ADVANCED("sg::file_writer(..)")(Catch::Benchmark::Chronometer meter) {
        std::vector<std::vector<uint64_t>> vec_data = random_data(500, 1500);

        meter.measure([path, &vec_data] {
            sg::file_writer writer;
            writer.start(path, nullptr, nullptr, nullptr);

            for (const auto& buf : vec_data) {
                auto data_buf = sg::make_shared_c_buffer<std::byte>(buf.size() * sizeof(uint64_t));
                std::memcpy(data_buf.get(), &buf[0], buf.size() * sizeof(uint64_t));
                writer.write_async(std::move(data_buf));
            }
            writer.stop();
        });
    };
}
