#pragma once

#include "buffer.h"
#include "sg/export/common.h"

#include <filesystem>
#include <string>

namespace sg::common::file {

[[nodiscard]] SG_COMMON_EXPORT unique_c_buffer<std::byte> get_contents(std::filesystem::path path);

}