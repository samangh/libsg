#pragma once

#include <cstdint>

namespace sg::checksum {

// You define additionalc custom CRCs easily
// see https://www.boost.org/doc/libs/1_84_0/doc/html/crc/crc_samples.html

/**
 * @brief crc32c Calculates CRC32-C checksum
 *
 * Calculates CRC32-C checksum, using polynomial 0x1EDC6F41 (or reversed representation,
 * 0x82F63B78).
 *
 * Assumtions:
 *  - Initial remainder:       0xFFFFFFFF
 *  - Reflect input bytes:     yes
 *  - Reflex output remainder: yes
 *  - Invert output remainder: yes (i.e. flip all bits, equivalent to XOR wih 0xFFFFFFFF)
 *
 * @param data      Input data to checksum
 * @param length    Length of input data
 * @return
 */
uint32_t crc32c(const void *data, uint32_t length);

uint32_t crc32(const void *data, uint32_t length);

uint16_t crc16(const void *data, uint32_t length);

} // namespace sg::checksum
