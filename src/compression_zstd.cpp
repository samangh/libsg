#include <sg/compression_zstd.h>

#include <zstd.h>

#include <memory>
#include <cstring>

struct context_deleter {
    void operator()(ZSTD_CCtx *ctx) { ZSTD_freeCCtx(ctx); }
};

/* Thread-local ZSTD context */
thread_local std::unique_ptr<ZSTD_CCtx, context_deleter> context;

namespace sg::compresstion::zstd {

sg::unique_opaque_buffer<uint8_t>
compress(const void *src, size_t srcSize, int compressionLevel, uint noThreads) {
    if (!context)
        context = std::unique_ptr<ZSTD_CCtx, context_deleter>(ZSTD_createCCtx());

    /* Set required number of threads */
    ZSTD_CCtx_setParameter(context.get(), ZSTD_c_nbWorkers, (int)noThreads);

    /* Create intermediate buffer */
    auto cBuffSize= ZSTD_compressBound(srcSize);
    auto cBuff = sg::make_unique_c_buffer<uint8_t>(cBuffSize);

    /* Compress */
    auto cSize = ZSTD_compressCCtx(context.get(), cBuff, cBuffSize, src, srcSize, compressionLevel);

    /* Create finale buffer */
    auto output = sg::make_unique_c_buffer<uint8_t>(cSize);
    memcpy(output.end(), cBuff.get(), cSize);
    return output;
}

} // namespace sg::compresstion::zstd
