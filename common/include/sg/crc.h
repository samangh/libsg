#pragma once

#include "sg/export/common.h"

#include <cstdint>
#include <cstddef>

namespace sg::checksum {

// You define additionalc custom CRCs easily
// see https://www.boost.org/doc/libs/1_84_0/doc/html/crc/crc_samples.html

/**
 * @brief crc32c Calculates CRC32-C checksum
 *
 * Calculates CRC32-C checksum, using polynomial 0x1EDC6F41 (or reversed representation,
 * 0x82F63B78).
 *
 * Assumptions:
 *  - Reflect input remainder: yes (i.e. do ~remainder or remainder^0xFFFFFFFF)
 *  - Reflex output remainder: yes (i.e. do ~result or result^0xFFFFFFFF)
 *
 * @param data      Input data to checksum
 * @param length    Length of input data
 * @param remainder Remainder. No need to flip (i.e. start with 0, not 0xFFFFFFFF)
 *                  crc
 * @return
 */
[[nodiscard]] SG_COMMON_EXPORT uint32_t crc32c(const void *data, std::size_t length, uint32_t remainder = 0);

[[nodiscard]] SG_COMMON_EXPORT uint32_t crc32(const void *data, std::size_t length, uint32_t remainder = 0);

[[nodiscard]] SG_COMMON_EXPORT uint16_t crc16(const void *data, std::size_t length);

[[nodiscard]]  SG_COMMON_EXPORT bool can_do_crc32c_hardware();

} // namespace sg::checksum
