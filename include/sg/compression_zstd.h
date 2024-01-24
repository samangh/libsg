#pragma once

#include "buffer.h"

#include <cstddef>

namespace sg::compression::zstd {

sg::unique_opaque_buffer<uint8_t>
compress(const void *src, size_t srcSize, int compressionLevel, int noThreads);

sg::unique_opaque_buffer<uint8_t>
compress(const sg::IBuffer<uint8_t>& srcBuffer, int compressionLevel, int noThreads);

}
