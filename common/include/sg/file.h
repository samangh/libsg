#pragma once

#include "buffer.h"
#include <sg/export/common.h>

#include <filesystem>
#include <string>

namespace sg::common::file {

[[nodiscard]] SG_COMMON_EXPORT unique_c_buffer<std::byte>
read(const std::filesystem::path& path,
             std::ios_base::openmode mode = std::ios::binary | std::ios::in);

SG_COMMON_EXPORT size_t read(const std::filesystem::path& path,
                                     std::byte* buffer,
                                     size_t count,
                                     std::ios_base::openmode mode = std::ios::binary |
                                                                    std::ios::in);

SG_COMMON_EXPORT void write(const std::filesystem::path& path,
                            const std::byte* buffer,
                            size_t count,
                            std::ios_base::openmode mode = std::ios::binary | std::ios::out);

SG_COMMON_EXPORT void write(const std::filesystem::path& path,
                            const sg::IBuffer<std::byte>& buffer,
                            std::ios_base::openmode mode = std::ios::binary | std::ios::out);

}