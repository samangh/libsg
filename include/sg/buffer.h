#pragma once

#include <stddef.h>
#include <memory>

namespace sg
{

template<typename T, typename del=std::default_delete<T>>
class unique_ptr_array {
    std::unique_ptr<T, del> ptr;
    size_t length;
public:
    unique_ptr_array(T* _ptr, size_t _length) noexcept:
        ptr(std::unique_ptr<T, del>(_ptr)), length(_length) {}
    unique_ptr_array(std::unique_ptr<T, del> _ptr, size_t _length) noexcept:
        ptr(_ptr), length(_length) {}

    // Move constrcutor
    unique_ptr_array(unique_ptr_array&&) = default;
    unique_ptr_array& operator=(unique_ptr_array&& data) = default;

    // Implicit cast
    operator T*() const noexcept{
        return ptr;
    }

    /* Return the stored pointer.*/
    T* get() const noexcept {
        return ptr.get();
    }

    /* Exchange the pointer and the associated length */
    T* swap(unique_ptr_array<T, del>& other) noexcept {
        ptr.swap(other.ptr);

        auto other_length = other.length;
        other.length = length;
        length = other_length;
    }

    /* Release ownership of any stored pointer. */
    void release() noexcept {
        ptr.release();
    }

    /* returns the number of elements in the array */
    size_t size() const noexcept {
        return length;
    }

    T* begin() const noexcept {
        return ptr.get();
    }

    T* end() const noexcept {
        return ptr.get() + length;
    }
};

}
