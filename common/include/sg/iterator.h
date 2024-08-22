#pragma once

#include <iterator>

namespace sg {

/* an iterator over a  contiguous container */
template <typename T>
class contiguous_iterator
{
    /* Implementation courtesy of Alex Panter, see
     * https://stackoverflow.com/questions/69890176/create-contiguous-iterator-for-custom-class */
  public:
    using iterator_category = std::contiguous_iterator_tag;
    using iterator_concept = std::contiguous_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::remove_cv_t<T>;
    using element_type = T;
    using pointer = T*;
    using reference = T&;

    // constructor for Array<T,S>::begin() and Array<T,S>::end()
    contiguous_iterator(pointer ptr) : mPtr(ptr) {}

    // std::weakly_incrementable<I>
    contiguous_iterator& operator++() {
        ++mPtr;
        return *this;
    }
    contiguous_iterator operator++(int) {
        contiguous_iterator tmp = *this;
        ++(*this);
        return tmp;
    }
    contiguous_iterator() : mPtr(nullptr) {}

    // std::input_or_output_iterator<I>
    reference operator*() { return *mPtr; }

    // std::indirectly_readable<I>
    friend reference operator*(const contiguous_iterator& it) { return *(it.mPtr); }

    // std::forward_iterator<I>
    bool operator==(const contiguous_iterator& it) const { return mPtr == it.mPtr; }

    // std::bidirectional_iterator<I>
    contiguous_iterator& operator--() { --mPtr; return *this; }
    contiguous_iterator operator--(int) { contiguous_iterator tmp = *this; --(*this); return tmp; }

    // std::random_access_iterator<I>
    //     std::totally_ordered<I>
    std::weak_ordering operator<=>(const contiguous_iterator& it) const =default;

    //     std::sized_sentinel_for<I, I>
    difference_type operator-(const contiguous_iterator& it) const { return mPtr - it.mPtr; }

    //     std::iter_difference<I> operators
    contiguous_iterator& operator+=(difference_type diff) { mPtr += diff; return *this; }
    contiguous_iterator& operator-=(difference_type diff) { mPtr -= diff; return *this; }
    contiguous_iterator operator+(difference_type diff) const { return contiguous_iterator(mPtr + diff); }
    contiguous_iterator operator-(difference_type diff) const { return contiguous_iterator(mPtr - diff); }
    friend contiguous_iterator operator+(difference_type diff, const contiguous_iterator& it) { return it + diff; }
    friend contiguous_iterator operator-(difference_type diff, const contiguous_iterator& it) { return it - diff; }
    reference operator[](difference_type diff) const { return mPtr[diff]; }

    // std::contiguous_iterator<I>
    pointer operator->() const { return mPtr; }
private:
    T* mPtr;
};

}
