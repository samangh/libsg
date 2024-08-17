#include "sg/bytes.h"

#include <algorithm>
#include <cstring>
#include <bit>

double sg::bytes::to_double(const std::byte *buff, std::endian src_endian) {
    double val;

    if (std::endian::native == src_endian)
        memcpy(&val, buff, sizeof(double));
    else
    {
        auto ptr = (std::byte*)&val;
        auto size = sizeof(double);
        for(size_t i=0; i < size; ++i)
            ptr[i]=buff[size-i-1];
    }
    return val;
}

std::array<std::byte, sizeof(double)> sg::bytes::to_bytes(double input,
                                                          std::endian endian) {
    auto bytes =
        std::bit_cast<std::array<std::byte, sizeof(double)>>(input);

    /* If system and the desired endian is different, then swap endianness */
    if (std::endian::native != endian)
        std::ranges::reverse(bytes);

    return bytes;
}
