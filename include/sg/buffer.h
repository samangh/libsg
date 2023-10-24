#pragma once

#include "sg/memory.h"
#include <memory>

namespace sg {

template <typename T, typename deleter = std::default_delete<T>> //
class unique_buffer {
    std::unique_ptr<T, deleter> ptr;
    size_t length;

  public:
    /* Default constructor, creates a unique_ptr that owns nothing.*/
    unique_buffer() noexcept //
        : length(0){};

    /* Constrcts from bare pointer */
    unique_buffer(T *_ptr, size_t _length) noexcept //
        : ptr(std::unique_ptr<T, deleter>(_ptr)), length(_length) {}

    /* Take owenership of bare pointer with custom deleter */
    unique_buffer(T *_ptr, size_t _length, const deleter &del) noexcept //
        : ptr(std::unique_ptr<T, deleter>(_ptr, del)), length(_length) {}

    /* Takes ownership of a unique-pointer */
    unique_buffer(std::unique_ptr<T, deleter> _ptr, size_t _length) noexcept //
        : ptr(_ptr), length(_length) {}

    // Move constrcutor
    unique_buffer(unique_buffer &&) = default;
    unique_buffer &operator=(unique_buffer &&data) = default;

    virtual ~unique_buffer() = default;

    // Implicit cast
    operator T *() const noexcept { return ptr; }

    /* Return the stored pointer.*/
    T *get() const noexcept { return ptr.get(); }

    /* Exchange the pointer and the associated length */
    T *swap(unique_buffer<T, deleter> &other) noexcept {
        ptr.swap(other.ptr);

        auto other_length = other.length;
        other.length = length;
        length = other_length;
    }

    /* Release ownership of any stored pointer. */
    void release() noexcept { ptr.release(); }

    /* Returns the number of elements */
    size_t size() const noexcept { return length; }

    /** Replace the stored pointer.
     *
     * The deleter will be invoked if a pointer is already owned.
     */
    void reset(T *_ptr, size_t _length) noexcept {
        ptr.reset(_ptr);
        this->length = _length;
    }

    /** Frees the stored pointer.
     *
     * The deleter will be invoked if a pointer is already owned.
     */
    void reset() noexcept {
        reset(nullptr, 0);
    }

    T *begin() const noexcept { return ptr.get(); }

    T *end() const noexcept { return ptr.get() + length; }
};

/* A version of unique_buffer that uses C-style free() to delete the base pointer */
template <typename T>
using unique_c_buffer= unique_buffer<T, deleter_free<T>>;

/* Allocates memoery and creates a unique_buffer that uses C-style free as deleter */
template <typename T>
unique_c_buffer<T> make_unique_c_buffer(size_t length)
{
    T* ptr = (T*)malloc(sizeof(T)*length);
    return unique_c_buffer<T>(ptr, length);

    /* Note: this could also be done by doing
     *
     *     return unique_buffer<T, decltype(&free)>(ptr, length, free);
     *
     * but that uses an additional 8-bytes of memory. */
}

} // namespace sg
