#include "sg/memory.h"

namespace sg::memory {

void* MallocOrThrow(size_t size) {
    if (size == 0)
        throw std::bad_alloc();

    void* result = malloc(size);
    if (!result)
        throw std::bad_alloc();

    return result;
}
void* ReallocOrFreeAndThrow(void* ptr, size_t size) {
    void* result;
    if (size == 0)
        goto error;

    result = realloc(ptr, size);
    if (!result)
        goto error;

    return result;

error:
    free(ptr);
    throw std::bad_alloc();
}
} // namespace sg::memory
