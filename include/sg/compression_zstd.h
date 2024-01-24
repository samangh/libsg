#pragma once

#include "buffer.h"

#include <cstddef>

namespace sg::compression::zstd {

/********************** Compression functions **********************/

sg::unique_opaque_buffer<uint8_t>
compress(const void *src, size_t srcSize, int compressionLevel, int noThreads);

template <typename T> sg::unique_opaque_buffer<uint8_t>
compress(const sg::IBuffer<T>& srcBuffer, int compressionLevel, int noThreads);

/********************** Decompression functions **********************/

sg::unique_opaque_buffer<uint8_t> decompress(const void *src, size_t srcSize);

template <typename T>
sg::unique_opaque_buffer<uint8_t> decompress(const sg::IBuffer<T>& srcBuffer);

}
