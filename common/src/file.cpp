#include "sg/file.h"

#include <fstream>

namespace sg::common::file {

std::string get_contents(std::filesystem::path path) {
    auto size = std::filesystem::file_size(path);

    std::ifstream stream;

    /* all errors are exceptions*/
    stream.exceptions(std::ifstream::badbit);

    /* open */
    stream.open(path, std::ios::binary | std::ios::in);

    /* get all data */
    std::string contents(size, '\0');
    stream.read(&contents[0], size);

    return contents;

}

} // namespace sg::common::file