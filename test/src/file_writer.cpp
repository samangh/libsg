#include <cstring>
#include <sg/file_writer.h>

#include <doctest/doctest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

static std::string read_file(std::string path) {
    std::ifstream t(path);
    return std::string((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
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
