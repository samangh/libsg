#include <sg/cpu.h>
#include <sg/crc.h>

#include "include/internal_crc32c.h"

#include <boost/crc.hpp>

namespace sg::checksum {

bool can_do_crc32c_hardware() {
#ifdef HAVE_HARDWARE_CRC32
    return true;
#else
    return false;
#endif
}

uint32_t crc32c(const void *data, uint32_t length) {
    /* Notes:
     *   - we invert (use ~) the result this is equivalent to XORing with 0xFFFFFFFF
     *   - the CRC32C initial remainder is 0xFFFFFFFFU */

    static const uint32_t initial_remainder = 0xFFFFFFFFU;

/* If available, use hardware assited version.
 *
 * We invert (use ~), this is equivalent to XORing with 0xFFFFFFFF */
#ifdef HAVE_HARDWARE_CRC32
    switch (sg::cpu::current_cpu_vendor()) {
    case sg::cpu::cpu_vendor::Amd:
        return ~_internal::crc32c_hardware_amd(data, length, initial_remainder);
    case sg::cpu::cpu_vendor::Intel:
        return ~_internal::crc32c_hardware_intel(data, length, initial_remainder);
    default:
        // For other vendors, go to tabular implementation
        break;
    }
#endif

    /* If can't use hardware, use 'Slicing' lookup tables*/
    return ~_internal::crc32c_tabular(data, length, initial_remainder);

    // Above tabular method is 10% more efficient
    // boost::crc_optimal<32, 0x1EDC6F41, 0xFFFFFFFFU, 0, true, true> crc;
    // crc.process_bytes(data, length);
    // return ~crc();
}

uint32_t crc32(const void *data, uint32_t length) {
    boost::crc_32_type crc;
    crc.process_bytes(data, length);
    return crc();
}

uint16_t crc16(const void *data, uint32_t length) {
    thread_local static boost::crc_16_type crc;
    crc.process_bytes(data, length);
    return crc();
}

} // namespace sg::checksum
