#include <sg/compression_zstd.h>

#include <zstd.h>

#include <memory>
#include <cstring>
#include <stdexcept>

#define ZSTD_THROW_ON_ERROR(fn)                              \
    do {                                                     \
        size_t const err = (fn);                             \
        if (ZSTD_isError(err))                               \
        throw std::exception(ZSTD_getErrorName(err));        \
    } while (0)

struct context_deleter {
    void operator()(ZSTD_CCtx *ctx) { ZSTD_freeCCtx(ctx); }
};

/* Thread-local settings, we set these only if there is a change,
 * as there is a performance penalty */
thread_local std::unique_ptr<ZSTD_CCtx, context_deleter> _context;
thread_local int _noThreads;
thread_local int _cLevel;

namespace sg::compression::zstd {

sg::unique_opaque_buffer<uint8_t>
compress(const void *src, size_t srcSize, int compressionLevel, int noThreads) {
    if (!_context)
        _context = std::unique_ptr<ZSTD_CCtx, context_deleter>(ZSTD_createCCtx());

    /* Set required number of threads */
    if (_noThreads != noThreads)
    {
        ZSTD_THROW_ON_ERROR(ZSTD_CCtx_setParameter(_context.get(), ZSTD_c_nbWorkers, noThreads));
        _noThreads = noThreads;
    }

    /* Set required number of compression level */
    if (_cLevel != compressionLevel)
    {
        ZSTD_THROW_ON_ERROR(ZSTD_CCtx_setParameter(_context.get(), ZSTD_c_compressionLevel, compressionLevel));
        _cLevel = compressionLevel;
    }


    /* Create intermediate buffer */
    auto cBuffSize= ZSTD_compressBound(srcSize);
    auto cBuff = sg::make_unique_c_buffer<uint8_t>(cBuffSize);

    /* Compress */
    auto cSize = ZSTD_compress2(_context.get(), cBuff, cBuffSize, src, srcSize);
    ZSTD_THROW_ON_ERROR(cSize);

    /* Create finale buffer */
    auto output = sg::make_unique_c_buffer<uint8_t>(cSize);
    memcpy(output.get(), cBuff.get(), cSize);
    return output;
}

} // namespace sg::compresstion::zstd
