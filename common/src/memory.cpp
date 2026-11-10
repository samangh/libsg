#include "sg/memory.h"
#include "sg/debug.h"

namespace sg::memory {

void* MallocOrThrow(size_t size) {
    if (size == 0)
        SG_THROW(std::bad_alloc);

    void* result = malloc(size);
    if (!result)
        SG_THROW(std::bad_alloc);

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
    SG_THROW(std::bad_alloc);
}
} // namespace sg::memory
