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
