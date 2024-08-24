#include <cstring>
#include <sg/file_writer_uv.h>

#include <doctest/doctest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

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
