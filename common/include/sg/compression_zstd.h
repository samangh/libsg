#pragma once

#include "buffer.h"

#include <cstddef>

namespace sg::compression::zstd {

/************************ Helper functions *************************/

[[nodiscard]] int min_compression_level();
[[nodiscard]] int max_compression_level();
[[nodiscard]] int default_compresssion_level();

/********************** Compression functions **********************/

/**
 *  @brief Compresses given object using ZStandard algorithm
 *
 *  @param  src       Source pointer
 *  @param  srcSize   Size of source data (in bytes, i.e. count * sizeof(..))
 *  @param  cLevel    Compression level
 *  @param  noThreads Number of threads to use
 *  @return buffer containing compressed data
 **/
[[nodiscard]] unique_c_buffer<std::byte> compress(const void *src, size_t srcSize, int cLevel, int noThreads);

/**
 *  @brief Compresses given buffer using ZStandard algorithm
 *
 *  @param  src       Source pointer
 *  @param  srcSize   Size of source data (in bytes, i.e. count * sizeof(..))
 *  @param  cLevel    Compression level
 *  @param  noThreads Number of threads to use
 *  @return buffer containing compressed data
 **/
template <typename T>
[[nodiscard]] unique_c_buffer<std::byte> compress(const sg::IBuffer<T> &srcBuffer,
                                    int compressionLevel,
                                    int noThreads) {
    return compress(srcBuffer.get(), srcBuffer.size() * sizeof(T), compressionLevel, noThreads);
}

/********************** Decompression functions **********************/
/**
 *  @brief recompresses given object using ZStandard algorithm
 *
 *  @param  src       compressed data pointer
 *  @param  srcSize   Size of source data (in bytes, i.e. count * sizeof(..))
 *  @return buffer containing de-compressed data
 **/
[[nodiscard]] unique_c_buffer<std::byte> decompress(const void *src, size_t srcSize);

/**
 *  @brief decompresses given object using ZStandard algorithm
 *
 *  @tparam T         Type to cast data to
 *  @param  src       compressed data pointer
 *  @param  srcSize   Size of source data (in bytes, i.e. count * sizeof(..))
 *  @return buffer containing de-compressed data
 **/
template <typename T>
[[nodiscard]] unique_c_buffer<T> decompress(const void *src, size_t srcSize) {
    auto ret = decompress(src, srcSize);
    auto count = ret.size();

    return unique_c_buffer<T>((T *)(ret.release()), count / sizeof(T));
}

/**
 *  @brief decompresses given buffer using ZStandard algorithm
 *
 *  @tparam T         Type to cast data to
 *  @param  src       compressed data buffer
 *  @return buffer containing de-compressed data
 **/
template <typename T>
[[nodiscard]] unique_c_buffer<T> decompress(const IBuffer<uint8_t> &src) {
    return decompress<T>(src.get(), src.size());
}

}  // namespace sg::compression::zstd
