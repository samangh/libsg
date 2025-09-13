#include "sg/file.h"

#include "sg/buffer.h"
#include "sg/memory.h"

#include <fstream>


namespace sg::common::file {

sg::unique_c_buffer<std::byte> get_contents(const std::filesystem::path& path) {
    auto const size = std::filesystem::file_size(path);

    auto data = sg::make_unique_c_buffer<std::byte>(size);
    get_contents(path, data.get(), size);

    return data;
}

size_t get_contents(const std::filesystem::path& path, std::byte* buffer, size_t count) {
    std::ifstream stream;

    /* all errors are exceptions*/
    stream.exceptions(std::ifstream::badbit);

    /* open */
    stream.open(path, std::ios::binary | std::ios::in);

    // note: technically size_t can hold a larger number than std::streamsize,
    // but std::numeric_limits<std::streamsize>::max() == 9223372 TB so I don't
    // think that it's going to be a problem!
    stream.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(count));

    return stream.gcount();
}

} // namespace sg::common::file