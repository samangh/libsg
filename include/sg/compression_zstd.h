#pragma once

#include "buffer.h"

#include <cstddef>

namespace sg::compression::zstd {

/************************ Helper functions *************************/

int min_compression_level();
int max_compression_level();
int default_compresssion_level();

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
unique_c_buffer<uint8_t>
compress(const void *src, size_t srcSize, int cLevel, int noThreads);

/**
 *  @brief Compresses given buffer using ZStandard algorithm
 *
 *  @param  src       Source pointer
 *  @param  srcSize   Size of source data (in bytes, i.e. count * sizeof(..))
 *  @param  cLevel    Compression level
 *  @param  noThreads Number of threads to use
 *  @return buffer containing compressed data
 **/
template <typename T> unique_c_buffer<uint8_t>
compress(const sg::IBuffer<T>& srcBuffer, int compressionLevel, int noThreads) {
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
unique_c_buffer<uint8_t> decompress(const void *src, size_t srcSize);

/**
 *  @brief recompresses given object using ZStandard algorithm
 *
 *  @tparam T         Type to cast data to
 *  @param  src       compressed data pointer
 *  @param  srcSize   Size of source data (in bytes, i.e. count * sizeof(..))
 *  @return buffer containing de-compressed data
 **/
template <typename T>
unique_c_buffer<T> decompress(const void *src, size_t srcSize){
    auto ret= decompress(src, srcSize);
    auto count = ret.size();

    return unique_c_buffer<T>((T*)(ret.release()), count/sizeof(T));
}

}
