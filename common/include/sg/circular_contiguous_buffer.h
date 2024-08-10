#pragma once

#include "sg/buffer.h"
#include "sg/export/sg_common.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

namespace sg {

/**
 * @brief a circular buffer where the the is contiguously in memory.
 * @details This uses twice as much memory as allocated, to increase performance. Adding more than
 * twice the size will cause the buffer to "shift" and so there is a memory copyign cost.
 *
 * If contiguous memory is not required, much better to use boost::circular_buffer instead.
 */
#include<vector>

template <typename T> class SG_COMMON_EXPORT circular_contiguous_buffer {
    size_t               m_cb_size;
    sg::unique_c_buffer<T> m_buff;

    size_t pos_begin{0}; // index of first element (i.e. similar to begin())
    size_t pos_end{0};   // index after last element (i.e. similar to end())
  public:
    circular_contiguous_buffer(size_t size)
        : m_cb_size(size),
          m_buff(sg::make_unique_c_buffer<T>(2*size)) {}

    size_t capacity() const { return m_cb_size; }
    size_t size() const { return pos_end - pos_begin; }

    T*       data() { return &m_buff[pos_begin]; }
    const T* data() const { return &m_buff[pos_begin]; }

    template <typename U>
    void push_back(U&& val) {
        if (pos_end != m_buff.size()) {
            m_buff[pos_end++] = std::forward<U>(val);
            if (pos_end - pos_begin > m_cb_size)
                ++pos_begin;
        } else {
            /* call destrctors of unneeded items, if they are non-trivial */
            if (!std::is_trivially_destructible_v<U>)
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

    void resize(size_t size) {
        /* call destrctors of unneeded items, if they are non-trivial */
        if (!std::is_trivially_destructible_v<T>)
            for (size_t i = 0; i < pos_begin; ++i)
                m_buff[i].~T();

        /* create new base buffer of new size-and swap */
        auto old_buf = sg::make_unique_c_buffer<T>(2*size);
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
        pos_begin = 0;
        pos_end = 0;
    }
};
} // namespace sg
