#pragma once

#include <cstddef>
namespace sg::compresstion::zstd {

void compress(void *dst, size_t dstCapacity, const void *src, size_t srcSize, int compressionLevel);

}
