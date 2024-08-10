#pragma once

#include "sg/export/sg_common.h"

#include <cstring>
#include <vector>

namespace sg {

/**
 * @brief a circular buffer where the the is contiguously in memory.
 * @details This uses twice as much memory as allocated, to increase performance. Adding more than
 * twice the size will cause the buffer to "shift" and so there is a memory copyign cost.
 *
 * If contiguous memory is not required, much better to use boost::circular_buffer instead.
 */
template <typename T> class SG_COMMON_EXPORT circular_contiguous_buffer {
    std::vector<T> m_vec; // Note: we don't use push_back(), etc so DON'T use at()!
    size_t         m_cb_size;
    size_t         m_vec_size;

    size_t pos_begin{0}; // index of first element (i.e. similar to begin())
    size_t pos_end{0};   // index after last element (i.e. similar to end())
  public:
    circular_contiguous_buffer(size_t size) : m_cb_size(size), m_vec_size(size + size) {
        m_vec.reserve(m_vec_size);
    }

    size_t   size() const { return m_cb_size; }
    T*       data() { return &m_vec[pos_begin]; }
    const T* data() const { return &m_vec[pos_begin]; }

    void push_back(const T& val) {
        if (pos_end != m_vec_size) {
            m_vec[pos_end++] = val;
            if (pos_end - pos_begin > m_cb_size)
                ++pos_begin;
        } else {
            std::memcpy(static_cast<void*>(&m_vec[0]),
                        static_cast<void*>(&m_vec[pos_begin]),
                        m_cb_size * sizeof(T));

            pos_end = pos_end - pos_begin;
            pos_begin = 0;

            push_back(val);
        }
    }
    void resize(size_t size) {
        if (size >= m_cb_size) {
            /* expansion */
            m_cb_size = size;
            m_vec_size = 2 * size;
            m_vec.reserve(m_vec_size);
        } else {
            /* Reduction. For a vector, the capacity is never reduced when resizing to smaller size,
             * so we copy the vector */

            m_cb_size = size;
            m_vec_size = 2 * size;

            /* figure out how many elements to copy */
            size_t copy_count = pos_end - pos_begin;
            if (copy_count >= m_cb_size)
                copy_count = m_cb_size;

            /* swap vectors */
            std::vector<T> vec_old;
            vec_old.swap(m_vec);

            m_vec.reserve(m_vec_size);
            std::memcpy(static_cast<void*>(&m_vec[0]),
                        static_cast<void*>(&vec_old[pos_end - copy_count]),
                        copy_count * sizeof(T));

            pos_begin = 0;
            pos_end = copy_count;
        }
    }
    void clear() {
        pos_begin = 0;
        pos_end = 0;
    }
};
} // namespace sg
