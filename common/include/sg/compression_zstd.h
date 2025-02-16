#pragma once

#include "sg/export/common.h"
#include "buffer.h"

#include <cstddef>

namespace sg::compression::zstd {

/************************ Helper functions *************************/

[[nodiscard]] SG_COMMON_EXPORT int min_compression_level();
[[nodiscard]] SG_COMMON_EXPORT int max_compression_level();
[[nodiscard]] SG_COMMON_EXPORT int default_compresssion_level();

[[nodiscard]] SG_COMMON_EXPORT size_t get_uncompressed_size(const void* src, size_t src_size);
[[nodiscard]] SG_COMMON_EXPORT size_t get_max_compressed_size(size_t src_size);

/********************** Compression functions **********************/

/**
 * @brief Compresses given object using ZStandard algorithm
 *
 *        Note: not all of the provider buffer may be used, look at the
 *        return value and reallocate if needed.
 *
 * @param  src       Source pointer
 * @param  srcSize   Size of source data (in bytes, i.e. count * sizeof(..))
 * @param  dst       Pointer to buffer store compressed data in
 * @param  dstSize   Size of the availble buffer
 * @param  cLevel    Compression level
 * @param  noThreads Number of threads to use
 * @return Number of bytes actually written to buffer
 */
[[nodiscard]] SG_COMMON_EXPORT size_t compress(const void *src, size_t srcSize, void *dst, size_t dstSize, int cLevel, int noThreads);

/**
 *  @brief Compresses given object using ZStandard algorithm
 *
 *  @param  src       Source pointer
 *  @param  srcSize   Size of source data (in bytes, i.e. count * sizeof(..))
 *  @param  cLevel    Compression level
 *  @param  noThreads Number of threads to use
 *  @return buffer containing compressed data
 **/
[[nodiscard]] SG_COMMON_EXPORT unique_c_buffer<std::byte> compress(const void *src, size_t srcSize, int cLevel, int noThreads);

template <typename RangeT>
    requires(std::ranges::contiguous_range<RangeT> &&
             std::is_trivially_copyable_v<std::ranges::range_value_t<RangeT>> &&
             std::has_unique_object_representations_v<std::ranges::range_value_t<RangeT>>)
[[nodiscard]] unique_c_buffer<std::byte>
compress(const RangeT& srcBuffer, int compressionLevel, int noThreads) {
    auto size = std::size(srcBuffer) * sizeof(std::ranges::range_value_t<RangeT>);
    return compress(std::begin(srcBuffer), size, compressionLevel, noThreads);
}

/********************** Decompression functions **********************/
void decompress(const void *src, size_t srcSize, void* dst, size_t uncompressedSize);

/**
 *  @brief recompresses given object using ZStandard algorithm
 *
 *  @param  src       compressed data pointer
 *  @param  srcSize   Size of source data (in bytes, i.e. count * sizeof(..))
 *  @return buffer containing de-compressed data
 **/
[[nodiscard]] SG_COMMON_EXPORT unique_c_buffer<std::byte> decompress(const void *src, size_t srcSize);

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
template <typename T = std::byte>
[[nodiscard]] unique_c_buffer<T> decompress(const IBuffer<std::byte> &src) {
    return decompress<T>(src.get(), src.size());
}

}  // namespace sg::compression::zstd
