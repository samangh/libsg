#pragma once

#include "buffer.h"
#include "sg/export/common.h"

#include <filesystem>
#include <string>

namespace sg::common::file {

[[nodiscard]] SG_COMMON_EXPORT unique_c_buffer<std::byte> get_contents(const std::filesystem::path& path);
SG_COMMON_EXPORT size_t get_contents(const std::filesystem::path& path, std::byte* buffer, size_t count);

}