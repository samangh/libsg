#pragma once

#include <cstdint>

namespace sg::checksum {
/**
 * @brief crc32c Calculates CRC32-C checksum
 *
 * Calculates CRC32-C checksum, using polynomial 0x82F63B78. Note that
 * most algorithms invert the final CRC remainder, you can do this by
 * using the `~` operation on the result.
 *
 * Some algorithms also invert the initial remainder.
 *
 * If checksumming a stream, you can can use the cehcksum result as the
 * 'remainder' of the next one (don't do any inverting if the spec asks
 * for it, except for the initial and final remainders). The final
 * result will be as if the data was checksummed whole.
 *
 * @param data      Input data to checksum
 * @param length    Length of input data
 * @param remainder Initial remainder, by default is 0xFFFFFFFF.
 * @return
 */
uint32_t crc32c(const void *data, uint32_t length, uint32_t remainder = 0xFFFFFFFF);
} // namespace sg::checksum
