#include "include/crc32c_hardware_32bit.h"
#include "include/crc32c_hardware_intel.h"
#include "include/crc32c_hardware_amd.h"
#include "include/crc32c_tabular.h"

#include <boost/crc.hpp>
#include <sg/bytes.h>
#include <sg/cpu.h>
#include <sg/crc.h>


namespace sg::checksum {

bool can_do_crc32c_hardware() {
#ifdef HAVE_HARDWARE_CRC32
    return true;
#else
    return false;
#endif
}

uint32_t crc32c(const void* data, std::size_t length, uint32_t remainder) {
    /* If available, use hardware assited version. Otherwise, use tabular verion. */
#ifdef HAVE_HARDWARE_CRC32
    #ifdef ENV_32BIT
    return ~crc32c_hardware_32bit(data, length, remainder);
    #else
    switch (sg::cpu::current_cpu_vendor()) {
    case sg::cpu::cpu_vendor::Amd:
        return ~crc32c_hardware_amd(data, length, remainder);
    case sg::cpu::cpu_vendor::Intel:
        return ~crc32c_hardware_intel(data, length, remainder);
    default:
        return ~crc32c_hardware_32bit(data, length, remainder);
    }
    #endif
#else
    static auto pTbl = compute_tabular_method_tables(0x82f63b78U);
    return ~crc32c_tabular(data, length, remainder, pTbl.get());
#endif
}

uint32_t crc32(const void* data, std::size_t length, uint32_t remainder) {
    static auto pTbl = compute_tabular_method_tables(0xEDB88320);
    return ~crc32c_tabular(data, length, remainder, pTbl.get());
}

uint16_t crc16(const void* data, std::size_t length) {
    thread_local static boost::crc_16_type crc;
    crc.process_bytes(data, length);
    return crc();
}

} // namespace sg::checksum
