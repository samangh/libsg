#include <sg/compression_zstd.h>
#include <zstd.h>

#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>

#define ZSTD_THROW_ON_ERROR(fn)                                                  \
    do {                                                                         \
        size_t const err = (fn);                                                 \
        if (ZSTD_isError(err)) throw std::runtime_error(ZSTD_getErrorName(err)); \
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

namespace sg::compression::zstd {

unique_c_buffer<std::byte> compress(const void *src,
                                    size_t srcSize,
                                    int compressionLevel,
                                    int noThreads) {
    if (!comp_context)
        comp_context = std::unique_ptr<ZSTD_CCtx, compression_context_deleter>(ZSTD_createCCtx());

    /* Set parameters */
    ZSTD_THROW_ON_ERROR(ZSTD_CCtx_setParameter(comp_context.get(), ZSTD_c_nbWorkers, noThreads));
    ZSTD_THROW_ON_ERROR(
        ZSTD_CCtx_setParameter(comp_context.get(), ZSTD_c_compressionLevel, compressionLevel));

    /* Create intermediate buffer */
    auto cBuffSize = ZSTD_compressBound(srcSize);
    ZSTD_THROW_ON_ERROR(cBuffSize);
    auto cBuff = sg::make_unique_c_buffer<uint8_t>(cBuffSize);

    /* Compress */
    auto cSize = ZSTD_compress2(comp_context.get(), cBuff.get(), cBuffSize, src, srcSize);
    ZSTD_THROW_ON_ERROR(cSize);

    /* Reallocate buffer */
    auto newPtr = sg::memory::ReallocOrFreeAndThrow(cBuff.release(), cSize);

    return sg::unique_c_buffer<std::byte>(static_cast<std::byte *>(newPtr), cSize);
}

unique_c_buffer<std::byte> decompress(const void *src, size_t srcSize) {
    if (!decomp_context)
        decomp_context =
            std::unique_ptr<ZSTD_DCtx, decompression_context_deleter>(ZSTD_createDCtx());

    /* Get size of original uncompressed data */
    auto unCompressedSize = ZSTD_getFrameContentSize(src, srcSize);
    if (unCompressedSize == ZSTD_CONTENTSIZE_ERROR)
        throw std::runtime_error("given data not compressed by zstd");
    if (unCompressedSize == ZSTD_CONTENTSIZE_UNKNOWN)
        throw std::runtime_error("zstd can't determine size of original uncompressed data");

    auto output = sg::make_unique_c_buffer<std::byte>(unCompressedSize);
    ZSTD_THROW_ON_ERROR(ZSTD_decompressDCtx(
        decomp_context.get(), (void *)(output.get()), unCompressedSize, src, srcSize));

    return output;
}

int min_compression_level() { return ZSTD_minCLevel(); }

int max_compression_level() { return ZSTD_maxCLevel(); }

int default_compresssion_level() { return ZSTD_defaultCLevel(); }

}  // namespace sg::compression::zstd
