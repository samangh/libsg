#pragma once

#include "sg/export/common.h"

#include <string>

namespace sg::common::hardware {

[[nodiscard]] SG_COMMON_EXPORT bool is_running_in_vm();

[[nodiscard]] SG_COMMON_EXPORT std::string bios_vendor();

}