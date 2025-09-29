#include "sg/file.h"

#include "sg/buffer.h"
#include "sg/memory.h"

#include <fstream>


namespace sg::common::file {

sg::unique_c_buffer<std::byte> read(const std::filesystem::path& path,
                                            std::ios_base::openmode mode) {
    auto const size = std::filesystem::file_size(path);

    auto data = sg::make_unique_c_buffer<std::byte>(size);
    read(path, data.get(), size, mode);

    return data;
}

size_t read(const std::filesystem::path& path, std::byte* buffer, size_t count,
                    std::ios_base::openmode mode) {
    std::ifstream stream;

    /* all errors are exceptions*/
    stream.exceptions(std::ifstream::badbit);

    /* open */
    stream.open(path, mode);

    // note: technically size_t can hold a larger number than std::streamsize,
    // but std::numeric_limits<std::streamsize>::max() == 9223372 TB so I don't
    // think that it's going to be a problem!
    stream.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(count));

    return stream.gcount();
}

void write(const std::filesystem::path& path, const std::byte* buffer, size_t count,
           std::ios_base::openmode mode) {
    std::ofstream stream;

    /* all errors are exceptions*/
    stream.exceptions(std::ifstream::badbit);

    /* open */
    stream.open(path, mode);

    // note: technically size_t can hold a larger number than std::streamsize,
    // but std::numeric_limits<std::streamsize>::max() == 9223372 TB so I don't
    // think that it's going to be a problem!
    stream.write(reinterpret_cast<const char*>(buffer), static_cast<std::streamsize>(count));
}

void write(const std::filesystem::path& path, const sg::IBuffer<std::byte>& buffer,
           std::ios_base::openmode mode) {
    write(path, buffer.get(), buffer.size(), mode);
}

} // namespace sg::common::file