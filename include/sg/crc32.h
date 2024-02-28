#pragma once

#include <cstdint>

namespace sg::checksum {
uint32_t crc32c(const void* data,
               uint32_t no_bytes,
               uint32_t previous_value,
               bool invert_previous_value = false, bool invert_final_value=true);
}
