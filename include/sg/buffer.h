#pragma once

#include <memory>

namespace sg {

template <typename T, typename deleter = std::default_delete<T>> //
class unique_ptr_with_size {
    std::unique_ptr<T, deleter> ptr;
    size_t length;

  public:
    /* Default constructor, creates a unique_ptr that owns nothing.*/
    unique_ptr_with_size() noexcept //
        : length(0){};

    /* Constrcts from bare pointer */
    unique_ptr_with_size(T *_ptr, size_t _length) noexcept //
        : ptr(std::unique_ptr<T, deleter>(_ptr)), length(_length) {}

    /* Take owenership of bare pointer with custom deleter */
    unique_ptr_with_size(T *_ptr, size_t _length, const deleter &del) noexcept //
        : ptr(std::unique_ptr<T, deleter>(_ptr, del)), length(_length) {}

    /* Takes ownership of a unique-pointer */
    unique_ptr_with_size(std::unique_ptr<T, deleter> _ptr, size_t _length) noexcept //
        : ptr(_ptr), length(_length) {}

    // Move constrcutor
    unique_ptr_with_size(unique_ptr_with_size &&) = default;
    unique_ptr_with_size &operator=(unique_ptr_with_size &&data) = default;

    virtual ~unique_ptr_with_size() = default;

    // Implicit cast
    operator T *() const noexcept { return ptr; }

    /* Return the stored pointer.*/
    T *get() const noexcept { return ptr.get(); }

    /* Exchange the pointer and the associated length */
    T *swap(unique_ptr_with_size<T, deleter> &other) noexcept {
        ptr.swap(other.ptr);

        auto other_length = other.length;
        other.length = length;
        length = other_length;
    }

    /* Release ownership of any stored pointer. */
    void release() noexcept { ptr.release(); }

    /* returns the number of elements in the array */
    size_t size() const noexcept { return length; }

    /** Replace the stored pointer.
     *
     * The deleter will be invoked if a pointer is already owned.
     */
    void reset(T *_ptr, size_t _length) noexcept {
        ptr.reset(_ptr);
        this->length = _length;
    }

    /** Replace the stored pointer.
     *
     * The deleter will be invoked if a pointer is already owned.
     */
    void reset(unique_ptr_with_size<T, deleter> other = unique_ptr_with_size<T, deleter>()) noexcept {
        ptr.reset(other.ptr);
        this->length = other.length;
    }

    T *begin() const noexcept { return ptr.get(); }

    T *end() const noexcept { return ptr.get() + length; }
};

/* Deleter that uses C-style free for freeing a pointer.
 *
 * Can be passed to std::unique_ptr etc. */
template <typename T> struct deleter_free {
    constexpr void operator()(T *p) const noexcept { free(p); }
};

/* Provides a version of unique_ptr_with_size that uses C-style free as deleter
 *
 * Could also be done by doing:
 *  unique_ptr_with_size<T, declfree(&free) (T *_ptr, size_t _length, free)
 * but that uses an additional 8-bytes of memory.
 */
template <typename T> //
class unique_ptr_with_size_free : public unique_ptr_with_size<T, deleter_free<T>> {
  public:
    /* Constrcts from bare pointer */
    unique_ptr_with_size_free(T *_ptr, size_t _length) noexcept
        : unique_ptr_with_size<T, deleter_free<T>>(_ptr, _length) {}

    /* Takes ownership of a unique-pointer */
    unique_ptr_with_size_free(std::unique_ptr<T> _ptr, size_t _length) noexcept
        : unique_ptr_with_size<T, deleter_free<T>>(_ptr.release(), _length) {}

    // Move constrcutor
    unique_ptr_with_size_free(unique_ptr_with_size_free &&) = default;
    unique_ptr_with_size_free &operator=(unique_ptr_with_size_free &&data) = default;
};

} // namespace sg
