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

template <typename T>
class rolling_contiguous_buffer {
    size_t m_cb_size;
    size_t m_max_size;
    double m_reserve_factor;

    std::vector<T> m_data;

    size_t pos_begin{0}; // index of first element (i.e. similar to begin())
    size_t pos_end{0};   // index after last element (i.e. similar to end())

  public:
    typedef std::size_t                  size_type;
    typedef contiguous_iterator<T>       iterator_type;
    typedef contiguous_iterator<const T> const_iterator_type;
    typedef T&                           reference;
    typedef const T&                     const_reference;

    explicit rolling_contiguous_buffer(size_t size, double reserveFactor = 1.0)
        : m_cb_size(size),
          m_max_size(size + static_cast<size_t>(size * reserveFactor)),
            m_reserve_factor(reserveFactor){
        static_assert(std::contiguous_iterator<iterator_type>);
        static_assert(std::contiguous_iterator<const_iterator_type>);
    }

    /* returns the capacity of the buffer */
    [[nodiscard]] size_t capacity() const { return m_cb_size; }
    [[nodiscard]] size_t size() const { return pos_end - pos_begin; }

    T*       data() { return &m_data[pos_begin]; }
    const T* data() const { return &m_data[pos_begin]; }

    void ensure_space(size_t noNewPoints) {
        /* check if reached maximum buffer size*/
        if (pos_end == m_max_size) {
            if (noNewPoints >= m_cb_size) {
                pos_begin=0;
                pos_end=0;

                m_data.clear();
                return;
            }

            if (m_reserve_factor >=1) {
                for (size_t i = 0; i < pos_begin+noNewPoints; ++i)
                    m_data[i].~T();

                std::memcpy(static_cast<void*>(m_data.data()),
                            static_cast<void*>(&m_data.data()[pos_begin + noNewPoints]),
                            (m_cb_size - noNewPoints) * sizeof(T));

                pos_begin=0;
                pos_end = m_cb_size - noNewPoints;

                m_data.erase(m_data.begin()+pos_end,m_data.end());
            }
            else {
                std::vector<T> newVec;
                newVec.reserve(m_cb_size);
                newVec.insert(newVec.begin(),
                              std::move_iterator(m_data.cbegin() + pos_begin + noNewPoints),
                              std::move_iterator(m_data.cbegin() + pos_end));
                m_data.swap(newVec);

                pos_begin=0;
                pos_end = m_data.size();
            }


            return;
        }

        /* if buffer has to grow, make sure it doesn't grow too much */
        if (m_data.size() + noNewPoints > m_data.capacity())
            if (m_data.capacity()*2 > m_max_size)
                m_data.reserve(m_max_size);
    }

    template <typename U> void push_back(U&& val) {
        ensure_space(1);
        m_data.emplace_back(std::forward<U>(val));

        // iterate end
        pos_end++;

        // iterate beginning
        if (pos_end - pos_begin > m_cb_size)
            ++pos_begin;
    }

    /**
     * @brief resize the buffer element count.
     * @details note that if size is smaller than the current size some elements from the front of
     * the buffer will be removed.
     * @param size the size to change to
     */
    void resize(size_t size, double reserveFactor = 1.0) {
        std::vector<T> newVec;

        auto toCopy = std::min(pos_end - pos_begin, size);
        newVec.insert(newVec.begin(), std::move(m_data.end() - toCopy), std::move(m_data.end()));
        newVec.swap(m_data);

        m_cb_size = size;
        m_max_size = size + static_cast<size_t>(reserveFactor * size);
        m_reserve_factor = reserveFactor;

        pos_begin = 0;
        pos_end = toCopy;
    }

    void clear() {
        m_data = std::vector<T>();
        pos_begin = 0;
        pos_end = 0;
    }

    /* iterators */
    [[nodiscard]] constexpr iterator_type begin() { return iterator_type(data()); }
    [[nodiscard]] constexpr iterator_type end() { return begin() + (pos_end - pos_begin); }

    /* const iterators */
    [[nodiscard]] constexpr const_iterator_type begin() const { return const_iterator_type(data()); }
    [[nodiscard]] constexpr const_iterator_type end() const { return begin() + (pos_end - pos_begin); }

    /* const iterators */
    [[nodiscard]] constexpr const_iterator_type cbegin() const { return const_iterator_type(data()); }
    [[nodiscard]] constexpr const_iterator_type cend() const { return cbegin() + (pos_end - pos_begin); }

    /* front/back */
    reference front() { return *begin(); }
    reference back() { return *(end() - 1); }

    /* const front/back */
    const_reference front() const {return *begin();}
    const_reference back() const {return *(end() - 1);;}

    const T& operator[](size_type i) const  { return m_data[pos_begin+i]; };
    T&       operator[](size_type i)  { return m_data[pos_begin+i]; }

    ~rolling_contiguous_buffer() {
        clear();
    }
};
} // namespace sg
