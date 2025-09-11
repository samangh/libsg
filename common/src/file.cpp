#include "sg/file.h"

#include "sg/buffer.h"
#include "sg/memory.h"

#include <fstream>


namespace sg::common::file {

sg::unique_c_buffer<std::byte> get_contents(std::filesystem::path path) {
    auto size = std::filesystem::file_size(path);

    std::ifstream stream;

    /* all errors are exceptions*/
    stream.exceptions(std::ifstream::badbit);

    /* open */
    stream.open(path, std::ios::binary | std::ios::in);

    auto data = sg::make_unique_c_buffer<std::byte>(size);
    stream.read(reinterpret_cast<char*>(data.get()), size);

    return data;
}

} // namespace sg::common::file