#pragma once

#include "sg/export/common.h"

#include <filesystem>
#include <string>

namespace sg::common::file {

[[nodiscard]] SG_COMMON_EXPORT std::string get_contents(std::filesystem::path path);

}