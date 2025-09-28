#include "include/crc32c_hardware_32bit.h"
#include "include/crc32c_hardware_intel.h"
#include "include/crc32c_hardware_armv7.h"
#include "include/crc32c_hardware_armv8.h"
#include "include/crc32c_tabular.h"

#include "include/crc32_hardware_armv7.h"

#include <boost/crc.hpp>
#include <sg/bytes.h>
#include <sg/cpu.h>
#include <sg/crc.h>


namespace sg::checksum {

bool can_do_crc32c_hardware() {
#ifdef CPU_SUPPORTS_SSE42
    return true;
#else
    return false;
#endif
}

uint32_t crc32c(const void* data, std::size_t length, uint32_t remainder) {
    /* If available, use hardware asssited version. Otherwise, use
     * tabular version. */
#if defined(HAVE_HARDWARE_CRC32C_64)
    return crc32c_impl(remainder, static_cast<const char*>(data), length);
#elif defined(HAVE_HARDWARE_CRC32C_32)
    return ~crc32c_hardware_32bit(data, length, ~remainder);
#elif defined(HAVE_HARDWARE_CRC32C_ARMV8)
    return ~crc32c_hardware_armv8(data, length, ~remainder);
#elif defined(HAVE_HARDWARE_CRC32C_ARMV7)
    return ~crc32c_hardware_armv7(data, length, ~remainder);
#else
    static auto pTbl = compute_tabular_method_tables(0x82f63b78U);
    return ~crc32c_tabular(data, length, ~remainder, pTbl.get());
#endif
}

uint32_t crc32(const void* data, std::size_t length, uint32_t remainder) {
#ifdef HAVE_HARDWARE_CRC32_ARMV7
    return ~crc32_hardware_armv7(data, length, ~remainder);
#else
    static auto pTbl = compute_tabular_method_tables(0xEDB88320);
    return ~crc32c_tabular(data, length, ~remainder, pTbl.get());
#endif
}

uint16_t crc16(const void* data, std::size_t length) {
    thread_local static boost::crc_16_type crc;
    crc.process_bytes(data, length);
    return crc();
}

} // namespace sg::checksum
