#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <stdexcept>

namespace sg {

/* Deleter that uses C-style free for freeing a pointer.
 *
 * Can be passed to std::unique_ptr etc. */
template <typename T>
struct deleter_free {
    constexpr void operator()(T* p) const noexcept { free(p); }
};

/* a class that will safely generate a weak_ptr that will be valid as
 * long as the class is in scope.
 *
 * If a class inherits or contains this object as a member, then this
 * can be used to track if that class is in scope. */
class enable_lifetime_indicator {
    /* note: This is similar to using std::enable_shared_from_this and
     * casting to a weak ptr, but in the case the weak_ptr would become
     * invalid if no other object is holding the shared_ptr */
   public:
    typedef std::nullptr_t item_type;
    [[nodiscard]] std::weak_ptr<item_type> get_lifetime_indicator() const noexcept {
        return _indicator;
    }
    virtual ~enable_lifetime_indicator() = default;

   private:
    std::shared_ptr<item_type> _indicator = std::make_shared<std::nullptr_t>();
};

}  // namespace sg

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
    if (size == 0) throw std::bad_alloc();

    void* result = malloc(size);
    if (!result) throw std::bad_alloc();

    return result;
}

static inline void* ReallocOrFreeAndThrow(void* ptr, size_t size) {
    void* result;
    if (size == 0) goto error;

    result = realloc(ptr, size);
    if (!result) goto error;

    return result;

error:
    free(ptr);
    throw std::bad_alloc();
}

/** Allocated memory of specific size, runs the specified function, and then clears the memory.
 *
 * On allocation error, throws std::bad_alloc and clears the memory. If #func throws an
 * exception, the memory is clear and the exception in re-thrown.
 *
 * @param[in]       size    amount of memory to allocation
 * @param[in,out]   memory  point to pointer to allocate
 * @param[in]       func    function to run after allocating memory
 *
 * @throw std::bad_alloc on failure to allocate storage.
 * @throw any other exceptions that @
 */
template <typename T>
inline void MallocAndFree(size_t size, T** memory, const std::function<void()>& func) {
    if (size == 0) throw std::bad_alloc();

    try {
        *memory = (T*)malloc(size);
        if (!*memory) throw std::bad_alloc();

        func();
        free(*memory);
    } catch (...) {
        if (*memory) free(*memory);
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
inline void CallocAndFree(size_t size, T** memory, const std::function<void()>& func) {
    if (size == 0) throw std::bad_alloc();

    try {
        *memory = (T*)malloc(size);
        if (!*memory) throw std::bad_alloc();
        func();
        free(*memory);
    } catch (...) {
        if (*memory) free(*memory);
        throw;
    }
}

}  // namespace sg::memory
