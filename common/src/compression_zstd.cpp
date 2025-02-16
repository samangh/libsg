#include <sg/compression_zstd.h>
#include <zstd.h>

#include <cstring>
#include <memory>
#include <stdexcept>

#define ZSTD_THROW_ON_ERROR(fn)                                                  \
    do {                                                                         \
        size_t const err = (fn);                                                 \
        if (ZSTD_isError(err)) throw std::runtime_error(ZSTD_getErrorName(err)); \
    } while (0)

namespace {

struct compression_context_deleter {
    void operator()(ZSTD_CCtx *ctx) { ZSTD_freeCCtx(ctx); }
};
struct decompression_context_deleter {
    void operator()(ZSTD_DCtx *ctx) { ZSTD_freeDCtx(ctx); }
};

}

namespace sg::compression::zstd {

size_t compress(const void *src, size_t srcSize, void *dst, size_t dstSize, int cLevel, int noThreads) {
    thread_local auto comp_context = std::unique_ptr<ZSTD_CCtx, compression_context_deleter>(ZSTD_createCCtx());

    /* Set parameters */
    ZSTD_THROW_ON_ERROR(ZSTD_CCtx_setParameter(comp_context.get(), ZSTD_c_nbWorkers, noThreads));
    ZSTD_THROW_ON_ERROR(
        ZSTD_CCtx_setParameter(comp_context.get(), ZSTD_c_compressionLevel, cLevel));

    /* Compress */
    auto cSize = ZSTD_compress2(comp_context.get(), dst, dstSize, src, srcSize);
    ZSTD_THROW_ON_ERROR(cSize);

    return cSize;
}

unique_c_buffer<std::byte> compress(const void *src,
                                    size_t srcSize,
                                    int compressionLevel,
                                    int noThreads) {
    /* Create intermediate buffer */
    auto cBuffSize = get_max_compressed_size(srcSize);
    auto cBuff = sg::make_unique_c_buffer<uint8_t>(cBuffSize);

    /* Compress */
    auto cSize = compress(src, srcSize, cBuff.get(), cBuffSize, compressionLevel, noThreads);

    /* Reallocate buffer */
    auto newPtr = sg::memory::ReallocOrFreeAndThrow(cBuff.release(), cSize);

    return sg::unique_c_buffer<std::byte>(static_cast<std::byte *>(newPtr), cSize);
}


void decompress(const void *src, size_t srcSize, void* dst, size_t uncompressedSize) {
    thread_local auto decomp_context =
        std::unique_ptr<ZSTD_DCtx, decompression_context_deleter>(ZSTD_createDCtx());

    ZSTD_THROW_ON_ERROR(ZSTD_decompressDCtx(
        decomp_context.get(), dst, uncompressedSize, src, srcSize));
}


unique_c_buffer<std::byte> decompress(const void *src, size_t srcSize) {

    /* Get size of original uncompressed data */
    auto unCompressedSize = get_uncompressed_size(src, srcSize);

    auto output = sg::make_unique_c_buffer<std::byte>(unCompressedSize);    
    decompress(src, srcSize, (void *)(output.get()), unCompressedSize);

    return output;
}

/************************ Helper functions *************************/

int min_compression_level() { return ZSTD_minCLevel(); }

int max_compression_level() { return ZSTD_maxCLevel(); }

int default_compresssion_level() { return ZSTD_defaultCLevel(); }

size_t get_uncompressed_size(const void* src, size_t src_size) {
    /* Get size of original uncompressed data */
    auto unCompressedSize = ZSTD_getFrameContentSize(src, src_size);
    if (unCompressedSize == ZSTD_CONTENTSIZE_ERROR)
        throw std::runtime_error("given data not compressed by zstd");
    if (unCompressedSize == ZSTD_CONTENTSIZE_UNKNOWN)
        throw std::runtime_error("zstd can't determine size of original uncompressed data");

    return unCompressedSize;
}

size_t get_max_compressed_size(size_t src_size) {
    auto size = ZSTD_compressBound(src_size);
    ZSTD_THROW_ON_ERROR(size);
    return size;
}

}  // namespace sg::compression::zstd
