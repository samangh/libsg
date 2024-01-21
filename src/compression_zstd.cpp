#include <sg/compression_zstd.h>

#include <zstd.h>

#include <memory>

struct context_deleter {
  void operator()(ZSTD_CCtx* ctx) {
      ZSTD_freeCCtx(ctx);
  }
};

thread_local std::unique_ptr<ZSTD_CCtx, context_deleter> context;

void sg::compresstion::zstd::compress(void *dst, size_t dstCapacity, const void *src, size_t srcSize, int compressionLevel)
{
    if (!context)
        context = std::unique_ptr<ZSTD_CCtx, context_deleter>(ZSTD_createCCtx());

    ZSTD_compressCCtx(context.get(), dst, dstCapacity,src, srcSize, compressionLevel);
}
