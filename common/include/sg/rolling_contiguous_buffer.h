#pragma once

#include "sg/buffer.h"
#include "sg/iterator.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

namespace sg {

/**
 * @brief a rolling buffer where the data is contiguously in memory.
 * @details This uses twice as much memory as allocated, to increase performance. Adding more than
 * twice the size will cause the buffer to "shift" and so there is a memory copying cost.
 *
 * If contiguous memory is not required, it might be better to use  std::queue, std:deque, or
 * boost::circular_buffer instead.
 */
template <typename T> class rolling_contiguous_buffer {
    size_t                 m_cb_size;
    sg::unique_c_buffer<T> m_buff;

    size_t pos_begin{0}; // index of first element (i.e. similar to begin())
    size_t pos_end{0};   // index after last element (i.e. similar to end())

  public:
    typedef std::size_t                 size_type;
    typedef contiguous_iterator<T>       iterator_type;
    typedef contiguous_iterator<const T> const_iterator_type;
    typedef T&                           reference;
    typedef const T&                     const_reference;

    explicit rolling_contiguous_buffer(size_t size)
        : m_cb_size(size),
          m_buff(sg::make_unique_c_buffer<T>(2 * size)) {
        static_assert(std::contiguous_iterator<iterator_type>);
        static_assert(std::contiguous_iterator<const_iterator_type>);
    }

    /* returns the capacity of the buffer */
    [[nodiscard]] size_t capacity() const { return m_cb_size; }
    [[nodiscard]] size_t size() const { return pos_end - pos_begin; }

    T*       data() { return &m_buff[pos_begin]; }
    const T* data() const { return &m_buff[pos_begin]; }

    template <typename U> void push_back(U&& val) {
        if (pos_end != m_buff.size()) {
            m_buff[pos_end++] = std::forward<U>(val);
            if (pos_end - pos_begin > m_cb_size)
                ++pos_begin;
        } else {
            for (size_t i = 0; i < pos_begin; ++i)
                m_buff[i].~T();

            std::memcpy(static_cast<void*>(&m_buff.get()[0]),
                        static_cast<void*>(&m_buff.get()[pos_begin]),
                        m_cb_size * sizeof(T));

            pos_end = pos_end - pos_begin;
            pos_begin = 0;

            push_back(val);
        }
    }

    /**
     * @brief resize the buffer element count.
     * @details note that if size is smaller than the current size some elements from the front of
     * the buffer will be removed.
     * @param size the size to change to
     */
    void resize(size_t size) {
        for (size_t i = 0; i < pos_begin; ++i)
            m_buff[i].~T();

        /* create new base buffer of new size-and swap */
        auto old_buf = sg::make_unique_c_buffer<T>(2 * size);
        m_buff.swap(old_buf);

        /* set new circular-buffer size */
        m_cb_size = size;

        /* figure out how many elements to copy */
        size_t copy_count = pos_end - pos_begin;
        if (copy_count >= m_cb_size)
            copy_count = m_cb_size;

        /* copy */
        std::memcpy(static_cast<void*>(&m_buff[0]),
                    static_cast<void*>(&old_buf[pos_end - copy_count]),
                    copy_count * sizeof(T));

        pos_begin = 0;
        pos_end = copy_count;
    }

    void clear() {
        for (size_t i = 0; i < pos_end; ++i)
            m_buff[i].~T();

        pos_begin = 0;
        pos_end = 0;
    }

    /* iterators */
    constexpr iterator_type begin() { return iterator_type(data()); }
    constexpr iterator_type end() { return begin() + (pos_end - pos_begin); }

    /* const interators */
    constexpr const_iterator_type begin() const { return const_iterator_type(data()); }
    constexpr const_iterator_type end() const { return begin() + (pos_end - pos_begin); }

    /* const interators */
    constexpr const_iterator_type cbegin() const { return const_iterator_type(data()); }
    constexpr const_iterator_type cend() const { return cbegin() + (pos_end - pos_begin); }

    /* front/back */
    reference front() { return *begin(); }
    reference back() { return *(end() - 1); }

    /* const front/back */
    const_reference front() const {return *begin();}
    const_reference back() const {return *(end() - 1);;}

    const T& operator[](size_type i) const  { return m_buff[pos_begin+i]; };
    T&       operator[](size_type i)  { return m_buff[pos_begin+i]; }

    ~rolling_contiguous_buffer() {
        clear();
    }
};
} // namespace sg
