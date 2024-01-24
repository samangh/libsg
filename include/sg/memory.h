#pragma once

#include <exception>
#include <functional>
#include <new>
#include <stdexcept>

namespace sg {

/* Deleter that uses C-style free for freeing a pointer.
 *
 * Can be passed to std::unique_ptr etc. */
template <typename T> struct deleter_free {
    constexpr void operator()(T *p) const noexcept { free(p); }
};

} // namespace sg

namespace sg::memory {

/**
 *  @brief Allocates memory using malloc, and throws an expcetion if there are issues.
 *
 *  @param[in]     size  size of memory to allocate
 *
 *  @returns pointer to memory location
 *
 *  @throw std::bad_alloc if size is zero
 *  @throw std::bad_alloc if can't allocate memory
 **/
static inline void* MallocOrThrow(size_t size) {
    if (size==0)
        throw std::bad_alloc();

    void* result = malloc(size);
    if (!result)
        throw std::bad_alloc();

    return result;
}

static inline void* ReallocOrFreeAndThrow(void* ptr, size_t size) {
    void* result;

    if (size==0)
        goto error;

    result = realloc(ptr, size);
    if (!result)
        goto error;

    return result;

 error:
    free(ptr);
    throw std::bad_alloc();
}

/** Allocated memory of specific size, runs the specified function, and then clears the memory.
 *
 * On allocation error, throws std::bad_alloc aand clears the memory. If #func throws an an
 * exception, the memory is clear and the exception in re-thrown.
 *
 * @param[in]       size    amount of memory to allocation
 * @param[in,out]   memory  point to pointer to allocate
 * @param[in]       func    function to run after allocating memoey
 *
 * @throw std::bad_alloc on failure to allocate storate.
 * @throw any other exceptons that @
 */
template <typename T>
inline void MallocAndFree(size_t size, T **memory, std::function<void()> func) {
    if (size==0)
        throw std::bad_alloc();

    try {
        *memory = (T *)malloc(size);
        if (!*memory)
            throw std::bad_alloc();

        func();
        free(*memory);
    } catch (...) {
        if (*memory)
            free(*memory);
        throw;
    }
}

/** Allocated memory of specific size, runs the specified function, and then clears the memory.
 *
 * On allocation error, throws std::bad_alloc aand clears the memory. If #func throws an an
 * exception, the memory is clear and the exception in re-thrown.
 *
 * @param[in]       size    amount of memory to allocation
 * @param[in,out]   memory  point to pointer to allocate
 * @param[in]       func    function to run after allocating memoey
 *
 * @throw std::bad_alloc on failure to allocate storate.
 * @throw any other exceptons that @
 */
template <typename T>
inline void CallocAndFree(size_t size, T **memory, std::function<void()> func) {
    if (size==0)
        throw std::bad_alloc();

    try {
        *memory = (T *)malloc(size);
        if (!*memory)
            throw std::bad_alloc();
        func();
        free(*memory);
    } catch (...) {
        if (*memory)
            free(*memory);
        throw;
    }
}

} // namespace sg::memory
