#include <cstring>
#include <sg/file_writer.h>

#include <doctest/doctest.h>

#include <fstream>
#include <string>
#include <cstring>

TEST_CASE("SG::common sg::filewiter: check async() write following by stop()") {
    std::string text = "TEST";

    /* using .stop() */
    {
        sg::file_writer writer;
        writer.start("test.txt", nullptr, nullptr, nullptr, 200);
        writer.write_async(text);
        writer.stop();

        std::ifstream     t("test.txt");
        std::stringstream buffer;
        buffer << t.rdbuf();

        CHECK(buffer.str().c_str() == text);
    }

    /* depending on constructor to flush */
    {
        {
            sg::file_writer writer;
            writer.start("test.txt", nullptr, nullptr, nullptr, 200);
            writer.write_async(text);
        }

        std::ifstream     t("test.txt");
        std::stringstream buffer;
        buffer << t.rdbuf();

        CHECK(buffer.str().c_str() == text);
    }
}
