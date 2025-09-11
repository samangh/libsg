#pragma once

#include "sg/buffer.h"
#include "sg/iterator.h"

#include <cstring>
#include <utility>
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
requires(std::contiguous_iterator<contiguous_iterator<T>> &&
    std::contiguous_iterator<contiguous_iterator<const T>>)
class rolling_contiguous_buffer {
    std::vector<T> m_data;

    size_t m_cb_size;
    size_t m_max_size;

    size_t pos_begin{0}; // index of first element (i.e. similar to begin())
    size_t pos_end{0};   // index after last element (i.e. similar to end())

    void advance_pos(size_t count) {
        // iterate end
        pos_end+=count;

        // iterate beginning
        if (pos_end - pos_begin > m_cb_size)
            pos_begin=pos_end - m_cb_size;;
    }

    void ensure_space(size_t noNewPoints) {
        /* if buffer is at max size */
        if (pos_end == m_max_size) {

            /* if the new number points is bigger than the store size,
             * just clear everything */
            if (noNewPoints >= m_cb_size) {
                pos_begin=0;
                pos_end=0;

                m_data.clear();
                return;
            }

            /* if there is enough reserve to just copy everything to the start */
            if (m_max_size - m_cb_size >=m_cb_size) {
                /* ensure destructor is called */
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

  public:
    typedef std::size_t                  size_type;
    typedef contiguous_iterator<T>       iterator_type;
    typedef contiguous_iterator<const T> const_iterator_type;
    typedef T&                           reference;
    typedef const T&                     const_reference;

    explicit rolling_contiguous_buffer(size_t size, size_t reserveSize)
        : m_cb_size(size),
          m_max_size(size + reserveSize) {}
    explicit rolling_contiguous_buffer(size_t size) : rolling_contiguous_buffer(size, size) {}

    /* returns the capacity of the buffer */
    [[nodiscard]] size_t capacity() const { return m_cb_size; }
    [[nodiscard]] size_t size() const { return pos_end - pos_begin; }

    T*       data() { return &m_data[pos_begin]; }
    const T* data() const { return &m_data[pos_begin]; }


    void push_back(const T& val) {
        ensure_space(1);
        m_data.push_back(val);

        advance_pos(1);
    }

    void push_back(T&& val) {
        emplace_back(std::forward<T>(val));
    }

    void emplace_back(T&& val) {
        ensure_space(1);
        m_data.emplace_back(std::forward<T>(val));

        advance_pos(1);
    }

    template <typename InputIt> void append(InputIt&& start, InputIt&& end) {
        auto count = std::distance(start,end);

        /* if insertion size is longer than store size, trim
         *
         * use std::cmp_greater for comparison due to size_t and iterator::difference_type being
         * different.
         */
        if (std::cmp_greater(count, m_cb_size)) {
            // trim
            std::advance(start, count - m_cb_size);
            count = m_cb_size;
        }

        ensure_space(count);
        m_data.insert(m_data.end(), std::forward<InputIt>(start), std::forward<InputIt>(end));

        advance_pos(count);
    }

    void append(std::initializer_list<T> ilist) {
        append(std::move_iterator(ilist.begin()), std::move_iterator(ilist.end()));
    }

    template <typename RangeT>
        requires(std::ranges::range<RangeT> &&
                 std::is_same_v<std::ranges::range_value_t<RangeT>, T>)
    void append(const RangeT& to_add) {
        append(to_add.begin(), to_add.end());
    }

    template <typename RangeT>
        requires(std::ranges::range<RangeT> &&
                 std::is_same_v<std::ranges::range_value_t<RangeT>, T> &&
                 !std::is_lvalue_reference_v<RangeT>)
    void append(RangeT&& to_add) {
        append(std::move_iterator(to_add.begin()), std::move_iterator(to_add.end()));
    }


    /**
     * @brief resize the buffer element count.
     * @details note that if size is smaller than the current size some elements from the front of
     * the buffer will be removed.
     * @param size the size to change to
     */
    void resize(size_t size) {
        resize(size,size);
    }

    void resize(size_t size, size_t reserve_size) {
        std::vector<T> newVec;

        auto toCopy = std::min(pos_end - pos_begin, size);
        newVec.insert(newVec.begin(), m_data.end() - toCopy, m_data.end());
        newVec.swap(m_data);

        m_cb_size = size;
        m_max_size = size + reserve_size;

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
