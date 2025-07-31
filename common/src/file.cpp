#include "sg/file.h"

#include <fstream>

namespace sg::common::file {

std::string get_contents(std::filesystem::path path) {
    std::string contents;
    std::ifstream stream;

    /* all errors are exceptions*/
    stream.exceptions(std::ifstream::badbit);

    /* open */
    stream.open(path, std::ios::binary | std::ios::in);

    /* get size */
    stream.seekg(0, std::ios::end);
    contents.resize(stream.tellg());

    /* seek to beginning and get data */
    stream.seekg(0, std::ios::beg);
    stream.read(&contents[0], contents.size());
    stream.close();

    return contents;
}

} // namespace sg::common::file