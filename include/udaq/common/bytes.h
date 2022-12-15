#pragma once

#include <vector>
#include <cstdint>
#include <type_traits>

namespace udaq::common::bytes {

enum class Endianess
{
    LittleEndian,
    BigEndian
};

uint16_t to_uint16(const uint8_t *buff, Endianess endian = Endianess::LittleEndian);
uint32_t to_uint32(const uint8_t *buff, Endianess endian = Endianess::LittleEndian);
int32_t to_int32(const uint8_t *buff, Endianess endian = Endianess::LittleEndian);
uint64_t to_uint64(const uint8_t *buff, Endianess endian = Endianess::LittleEndian);
int64_t to_int64(const uint8_t *buff, Endianess endian = Endianess::LittleEndian);
double to_double(const uint8_t *buff, Endianess endian = Endianess::LittleEndian);

template<typename T>
std::vector<uint8_t> to_bytes(T input, udaq::common::bytes::Endianess endian,
                                      typename std::enable_if<std::is_integral<T>::value, std::nullptr_t>::type = nullptr)
{
    /* Defined in header as this is a template, and so if defined in the
     * library we won't know about what types to compile for! */

   auto no_bytes = sizeof(T);
   std::vector<uint8_t> result(no_bytes);
   if (endian == udaq::common::bytes::Endianess::BigEndian)
       for (unsigned int i = 0; i < no_bytes; i++)
           result[no_bytes-1-i] = (uint8_t)(input >> (i*8));
   else
       for (unsigned int i = 0; i < no_bytes; i++)
           result[i] = (uint8_t)(input >> (i * 8));

   return result;
}

std::vector<uint8_t> to_bytes(double input, udaq::common::bytes::Endianess endian);

}
