#pragma once

#include "sg/memory.h"

#include <memory>
#include <type_traits>

namespace sg {

/* This file defines the following:
 *
 *  IBuffer<T>
 *      interface class for a generic buffer
 *
 *  unique_buffer<T, deleter>
 *      a buffer that uses a std::unique_ptr to hold the raw buffer, with a specific deleter.
 *
 *  shared_buffer<T, deleter>
 *      a buffer that uses a std::shared_ptr to hold the raw buffer, with a specific deleter.
 *
 *  unique_opaque_buffer<IBuffer<T>>
 *      opaque version of unique_buffer
 *
 *  shared_opaque_buffer<IBuffer<T>>
 *      opaque version of shared_buffer
 *
 * The deleter in the above classes allow you to specify how the object should be deleted, for
 * example using free(), delete, delete[], or some other function. By default, if no deleter is
 * specified then delete or delete[] is called depending on type of T. If sg::deleter_free is passed
 * then free() is used.
 *
 * The problem is the deleter becomes part of the class definition, so if you want to pass the
 * buffer to another function, and if you want the function to be generic so that it does not know
 * the deleter type, then buffer needs to be wrapped in another smart pointer. This is what the
 * unique_opaque_buffer and shared_opaque_buffer classes do.
 *
 * Notices:
 *
 *  - If your function is meant to take generic buffers with different possible deleter types,
 *    ALWAYS use the opaque buffers.
 *  - If you know (or what to ensure) the deleter type, then use the more concrete buffer types.
 *
 */

/* Interface for all buffer classes */
template <typename T> class IBuffer {
  public:
    virtual ~IBuffer() = default;

    /* Return the stored pointer.*/
    virtual T *get() const noexcept = 0;

    /* Returns the number of elements */
    virtual size_t size() const noexcept = 0;

    /** Frees the stored pointer.
     *
     * The deleter will be invoked if a pointer is already owned.
     */
    virtual void reset() noexcept = 0;
    /** Replace the stored pointer.
     *
     * The deleter will be invoked if a pointer is already owned.
     */
    virtual void reset(T *, size_t) noexcept = 0;

    virtual T *begin() const noexcept = 0;
    virtual T *end() const noexcept = 0;

    virtual T operator[](int i) const = 0;
    virtual T &operator[](int i) = 0;
};

template <typename T> class buffer_base : public IBuffer<T> {
  protected:
    IBuffer<T> *ptr;
    buffer_base(IBuffer<T> *buff) : ptr(buff) {}

  public:
    T *operator->() const noexcept { return ptr->get(); }

    virtual T *get() const noexcept override { return ptr->get(); }
    virtual size_t size() const noexcept override { return ptr->size(); }
    virtual void reset() noexcept override { ptr->reset(); }
    virtual void reset(T *_ptr, size_t _length) noexcept override {
        return ptr->reset(_ptr, _length);
    }

    virtual T *begin() const noexcept override { return ptr->begin(); };
    virtual T *end() const noexcept override { return ptr->end(); };

    virtual T operator[](int i) const override { return ptr->get()[i]; };
    virtual T &operator[](int i) override { return ptr->get()[i]; };
};

/* Creates unique opaque buffer */
template <typename T> class unique_opaque_buffer : public buffer_base<T> {
  private:
    std::unique_ptr<IBuffer<T>> buffer;

  public:
    /* Constructs opaque unique buffer.
     *
     * Notes:
     *
     *  - only passing objects of type unique_buffer<T> makes sense
     *  - the buffer MUST be generated on the heap */
    unique_opaque_buffer(IBuffer<T> *buff)
        : buffer_base<T>(buff),
          buffer(std::unique_ptr<IBuffer<T>>(buff)) {}

    IBuffer<T>* release() noexcept {
        return buffer.release();
    }
};

/* Creates shared opaque buffer */
template <typename T> class shared_opaque_buffer : public buffer_base<T> {
  private:
    std::shared_ptr<IBuffer<T>> buffer;

  public:
    /* Constructs opaque unique buffer.
     *
     * Notes:
     *
     *  - only passing objects of type shared_buffer<T> makes sense
     *  - the buffer MUST be generated on the heap */
    shared_opaque_buffer(IBuffer<T> *buff)
        : buffer_base<T>(buff),
          buffer(std::shared_ptr<IBuffer<T>>(buff)) {}
};

template <typename T, typename deleter = std::default_delete<T>> //
class unique_buffer : public IBuffer<T> {
    std::unique_ptr<T, deleter> ptr;
    size_t length;

  public:
    /* Default constructor, creates a unique_ptr that owns nothing.*/
    unique_buffer() noexcept //
        : length(0){};

    /* Constrcts from bare pointer */
    unique_buffer(T *_ptr, size_t _length) noexcept
        : ptr(std::unique_ptr<T, deleter>(_ptr)),
          length(_length) {}

    /* Take owenership of bare pointer with custom deleter */
    unique_buffer(T *_ptr, size_t _length, const deleter &del) noexcept
        : ptr(std::unique_ptr<T, deleter>(_ptr, del)),
          length(_length) {}

    /* Takes ownership of a unique-pointer */
    unique_buffer(std::unique_ptr<T, deleter> _ptr, size_t _length) noexcept
        : ptr(_ptr),
          length(_length) {}

    // Move constrcutor
    unique_buffer(unique_buffer &&) = default;
    unique_buffer &operator=(unique_buffer &&data) = default;

    T *get() const noexcept override { return ptr.get(); }

    /* Exchange the pointer and the associated length */
    T *swap(unique_buffer<T, deleter> &other) noexcept {
        ptr.swap(other.ptr);

        auto other_length = other.length;
        other.length = length;
        length = other_length;
    }

    /* Release ownership of any stored pointer. */
    void* release() noexcept {return ptr.release(); }

    size_t size() const noexcept override { return length; }

    void reset(T *_ptr, size_t _length) noexcept override {
        ptr.reset(_ptr);
        this->length = _length;
    }
    void reset() noexcept override { reset(nullptr, 0); }

    T *begin() const noexcept override { return ptr.get(); }
    T *end() const noexcept override { return ptr.get() + length; }

    T operator[](int i) const override { return (ptr.get())[i]; };
    T &operator[](int i) override { return (ptr.get())[i]; };
};

template <typename T, typename deleter = std::default_delete<T>> //
class shared_buffer : public IBuffer<T> {
    std::shared_ptr<T> ptr;
    size_t length;

  public:
    /* Default constructor, creates a unique_ptr that owns nothing.*/
    shared_buffer() noexcept : length(0){};

    /* Constrcts from bare pointer */
    shared_buffer(T *_ptr, size_t _length) noexcept
        : ptr(std::shared_ptr<T>(_ptr, deleter())),
          length(_length) {}

    /* Takes ownership of a shared-pointer */
    shared_buffer(std::shared_ptr<T> _ptr, size_t _length) noexcept //
        : ptr(_ptr),
          length(_length) {}

    // Move constrcutors
    shared_buffer(shared_buffer &&) = default;
    shared_buffer &operator=(shared_buffer &&data) = default;

    shared_buffer(shared_buffer &) = default;
    shared_buffer &operator=(shared_buffer &data) = default;

    operator T *() const noexcept override { return ptr.get(); }
    T *get() const noexcept override { return ptr.get(); }

    /* Exchange the pointer and the associated length */
    T *swap(shared_buffer<T, deleter> &other) noexcept {
        ptr.swap(other.ptr);

        auto other_length = other.length;
        other.length = length;
        length = other_length;
    }

    size_t size() const noexcept override { return length; }

    void reset(T *_ptr, size_t _length) noexcept override {
        ptr.reset(_ptr);
        this->length = _length;
    }
    void reset() noexcept override { reset(nullptr, 0); }

    virtual T *begin() const noexcept override { return ptr.get(); }
    virtual T *end() const noexcept override { return ptr.get() + length; }

    virtual T operator[](int i) const override { return (ptr.get())[i]; };
    virtual T &operator[](int i) override { return (ptr.get())[i]; };
};

/* A version of unique_buffer that uses C-style free() to delete the base pointer */
template <typename T> using unique_c_buffer = unique_buffer<T, deleter_free<T>>;

/* A version of shared_buffer that uses C-style free() to delete the base pointer */
template <typename T> using shared_c_buffer = shared_buffer<T, deleter_free<T>>;

/* Allocates memoery and creates a unique_buffer that uses C-style free as deleter */
template <typename T> unique_opaque_buffer<T> make_unique_c_buffer(size_t length) {
    T *ptr = (T *)memory::MallocOrThrow(sizeof(T) * length);
    auto base = new unique_buffer<T, deleter_free<T>>(ptr, length);
    return unique_opaque_buffer<T>(base);

    /* Note: this could also be done by doing
     *
     *     return unique_buffer<T, decltype(&free)>(ptr, length, free);
     *
     * but that uses an additional 8-bytes of memory. */
}

/* Allocates memoery and creates a unique_buffer that uses C-style free as deleter */
template <typename T> shared_opaque_buffer<T> make_shared_c_buffer(size_t length) {
    T *ptr = (T *) memory::MallocOrThrow(sizeof(T) * length);
    auto base = new shared_buffer<T, deleter_free<T>>(ptr, length);
    return shared_opaque_buffer<T>(base);

    /* Note: this could also be done by doing
     *
     *     return unique_buffer<T, decltype(&free)>(ptr, length, free);
     *
     * but that uses an additional 8-bytes of memory. */
}

} // namespace sg
