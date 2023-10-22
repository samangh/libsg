#pragma once

#include <stddef.h>
#include <memory>

namespace sg
{


template<typename T>
class unique_ptr_array {
    std::unique_ptr<T[]> ptr;
    size_t length;
public:
    unique_ptr_array(T* _ptr, size_t _length) noexcept:
        ptr(std::unique_ptr<T[]>(_ptr)), length(_length) {}
    unique_ptr_array(std::unique_ptr<T[]> _ptr, size_t _length) noexcept:
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
    T* swap(unique_ptr_array<T>& other) noexcept {
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

/* represents a buffer */
template <typename T>
struct buffer {
    buffer(T* _data, size_t _length): data(_data), length(_length){};
    T* data;
    size_t length;
};

/* Represents a buffer with 'unique' ownership.
 *
 * The underlying pointer will be freed when this buffer isdestructed.
 *
 */
template <typename T>
struct unique_buffer : buffer<T> {
    unique_buffer(T* _data, size_t _length): buffer<T>(_data, _length){}
    ~unique_buffer()
    {
        if (this->data !=nullptr)
        {
            free(this->data);
            this->length =0;
        }
    }

    unique_buffer(unique_buffer&& b) noexcept : buffer<T>(b.data, b.length)
    {
        b.data=nullptr;
        b.length=0;
    }
    unique_buffer& operator=(unique_buffer&& b) noexcept
    {
        this->data=b.data;
        this->length=b.length;

        b.data=nullptr;
        b.length=0;

        return *this;
    }

    unique_buffer(const unique_buffer&) = delete;
    unique_buffer& operator=(unique_buffer& b) = delete;

};
}
