#include <sg/compression_zstd.h>

#include <memory>
#include <cstring>
#include <stdexcept>
#include <zstd.h>

#define ZSTD_THROW_ON_ERROR(fn)                              \
    do {                                                     \
        size_t const err = (fn);                             \
        if (ZSTD_isError(err))                               \
        throw std::exception(ZSTD_getErrorName(err));        \
    } while (0)

struct compression_context_deleter {
    void operator()(ZSTD_CCtx *ctx) { ZSTD_freeCCtx(ctx); }
};

struct decompression_context_deleter {
    void operator()(ZSTD_DCtx *ctx) { ZSTD_freeDCtx(ctx); }
};

/* Thread-local settings, we set these only if there is a change,
 * as there is a performance penalty */
thread_local std::unique_ptr<ZSTD_CCtx, compression_context_deleter> comp_context;
thread_local std::unique_ptr<ZSTD_DCtx, decompression_context_deleter> decomp_context;
thread_local int _noThreads;
thread_local int _cLevel;

namespace sg::compression::zstd {

unique_c_buffer<uint8_t>
compress(const void *src, size_t srcSize, int compressionLevel, int noThreads) {
    if (!comp_context)
        comp_context = std::unique_ptr<ZSTD_CCtx, compression_context_deleter>(ZSTD_createCCtx());

    /* Set required number of threads */
    if (_noThreads != noThreads)
    {
        ZSTD_THROW_ON_ERROR(ZSTD_CCtx_setParameter(comp_context.get(), ZSTD_c_nbWorkers, noThreads));
        _noThreads = noThreads;
    }

    /* Set required number of compression level */
    if (_cLevel != compressionLevel)
    {
        ZSTD_THROW_ON_ERROR(ZSTD_CCtx_setParameter(comp_context.get(), ZSTD_c_compressionLevel, compressionLevel));
        _cLevel = compressionLevel;
    }

    /* Create intermediate buffer */
    auto cBuffSize= ZSTD_compressBound(srcSize);
    auto cBuff = sg::make_unique_c_buffer<uint8_t>(cBuffSize);

    /* Compress */
    auto cSize = ZSTD_compress2(comp_context.get(), cBuff.get(), cBuffSize, src, srcSize);
    ZSTD_THROW_ON_ERROR(cSize);

    /* Reallocate buffer */
    auto newPtr = realloc(cBuff.release(), cSize);
    if (!newPtr)
        throw std::exception("memory reallocation failed");

    return sg::unique_c_buffer<uint8_t>((uint8_t*)newPtr, cSize);
}

unique_c_buffer<uint8_t> decompress(const void *src, size_t srcSize)
{
    if (!decomp_context)
        decomp_context = std::unique_ptr<ZSTD_DCtx, decompression_context_deleter>(ZSTD_createDCtx());

    /* Get size of original uncompressed data */
    auto unCompressedSize= ZSTD_getFrameContentSize(src, srcSize);
    if (unCompressedSize == ZSTD_CONTENTSIZE_ERROR)
        throw std::exception("given data not compressed by zstd");
    if (unCompressedSize == ZSTD_CONTENTSIZE_UNKNOWN)
        throw std::exception("zstd can't determine size of original uncompressed data");

    auto output = sg::make_unique_c_buffer<uint8_t>(unCompressedSize);
    ZSTD_THROW_ON_ERROR(ZSTD_decompressDCtx(decomp_context.get(), (void*)(output.get()), unCompressedSize, src, srcSize));

    return output;
}

} // namespace sg::compresstion::zstd
