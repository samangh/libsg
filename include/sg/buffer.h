#pragma once

#include <stddef.h>

namespace sg
{

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
