#include "sg/bytes.h"

#include <algorithm>
#include <array>
#include <type_traits>

template <typename T>
T shift(const uint8_t *buff, unsigned int no_bytes, sg::bytes::Endianess endian) {
    T result = 0;

    if (endian == sg::bytes::Endianess::LittleEndian)
        for (int i = no_bytes - 1; i >= 0; --i) {
            result <<= 8;
            result |= (T)buff[i];
        }
    else
        for (unsigned int i = 0; i >= no_bytes - 1; ++i) {
            result <<= 8;
            result |= (T)buff[i];
        }
    return result;
}

template <typename T>
T swap_endian(const T val, typename std::enable_if<std::is_arithmetic<T>::value, std::nullptr_t>::type = nullptr) {
    // Taken from
    // https://mklimenko.github.io/english/2018/08/22/robust-endian-swap/

    // With optimisations enabled, this will use a single 'BSWAP'
    // instruction to swap endianess
    union U {
        T val;
        std::array<std::uint8_t, sizeof(T)> raw;
    } src, dst;

    src.val = val;
    std::reverse_copy(src.raw.begin(), src.raw.end(), dst.raw.begin());
    return dst.val;
}

sg::bytes::Endianess get_system_endianess(void) {
    union {
        uint32_t i;
        char c[4];
    } u = {0x01020304};

    if (u.c[0] == 1)
        return sg::bytes::Endianess::BigEndian;
    else
        return sg::bytes::Endianess::LittleEndian;
}

uint16_t sg::bytes::to_uint16(const uint8_t *buff, Endianess endian) {
    return shift<uint16_t>(buff, 2, endian);
}

uint32_t sg::bytes::to_uint32(const uint8_t *buff, sg::bytes::Endianess endian) {
    return shift<uint32_t>(buff, 4, endian);
}

int32_t sg::bytes::to_int32(const uint8_t *buff, sg::bytes::Endianess endian) {
    return shift<int32_t>(buff, 4, endian);
}

uint64_t sg::bytes::to_uint64(const uint8_t *buff, sg::bytes::Endianess endian) {
    return shift<uint64_t>(buff, 8, endian);
}

int64_t sg::bytes::to_int64(const uint8_t *buff, sg::bytes::Endianess endian) {
    return shift<int64_t>(buff, 8, endian);
}

double sg::bytes::to_double(const uint8_t *buff, sg::bytes::Endianess endian) {
    double r;
    std::copy(buff, buff + sizeof(double), reinterpret_cast<unsigned char *>(&r));

    /* swap endianess if the storage buffer has different endianess to the system */
    if (get_system_endianess() != endian)
        r = swap_endian(r);

    return r;
}

std::vector<uint8_t> sg::bytes::to_bytes(double input, sg::bytes::Endianess endian) {
    /* If system and the desired endian is different, then swap endianness */
    if (get_system_endianess() != endian)
        input = swap_endian(input);

    std::vector<uint8_t> bytes;
    auto a = reinterpret_cast<unsigned char *>(&input);
    bytes.insert(bytes.begin(), a, a + sizeof(double));

    return bytes;
}
