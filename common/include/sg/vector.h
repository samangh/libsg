#pragma once

#include <vector>
#include <ranges>

namespace sg::internal {
/****************** copy to iterator ******************/

template <std::input_iterator It, typename RangeT>
    requires(std::ranges::range<RangeT> &&
             std::is_same_v<std::ranges::range_value_t<RangeT>, std::iter_value_t<It>>)
It flatten_copy_to_iterator(It& it, RangeT&& buffers) {
    return std::copy(std::begin(buffers), std::end(buffers), it);
}

template <std::input_iterator It,
          typename RangeRangeT,
          typename RangeT = std::ranges::range_value_t<RangeRangeT>>
    requires(std::ranges::range<RangeRangeT> && std::ranges::range<RangeT> &&
             !std::is_same_v<RangeT, std::iter_value_t<It>>)
It flatten_copy_to_iterator(It& it, RangeRangeT&& buffers) {
    for (const auto& range : buffers) it = flatten_copy_to_iterator(it, range);
    return it;
}
} // namespace sg::internal

namespace sg::vector {

template <typename T, typename RangeT>
requires(std::ranges::range<RangeT> &&
         std::is_same_v<std::ranges::range_value_t<RangeT>,T>)
void append(std::vector<T> &base, const RangeT& to_add) {
    base.insert(std::end(base), std::begin(to_add), std::end(to_add));
}

template <typename T, typename RangeT>
requires(std::ranges::range<RangeT> &&
         std::is_same_v<std::ranges::range_value_t<RangeT>,T>&&
         !std::is_lvalue_reference_v<RangeT>)
void append(std::vector<T> &base, RangeT&& to_add) {
    /* in template, use !std::is_lvalue_reference_v<RangeT> to ensure that the
     * input is not an lvalue.
     *
     * Note that std::is_lvalue_reference_v<RangeT>  will not work, see
     * https://stackoverflow.com/questions/53758796/why-does-stdis-rvalue-reference-not-do-what-it-is-advertised-to-do
     */

    base.insert(std::end(base), make_move_iterator(std::begin(to_add)), make_move_iterator(std::end(to_add)));
}

/**
 * @brief Allocator adaptor that interposes construct() calls to
 *        convert value initialization into default initialization.
 *
 *        Pass to std::vector, to create a vector that does not initialise
 *        the values.
 */
template <typename T, typename A = std::allocator<T>> class default_init_allocator : public A {
    /* From:
     * https://stackoverflow.com/questions/21028299/is-this-behavior-of-vectorresizesize-type-n-under-c11-and-boost-container/21028912
     */
    typedef std::allocator_traits<A> a_t;

  public:
    template <typename U> struct rebind {
        using other = default_init_allocator<U, typename a_t::template rebind_alloc<U>>;
    };

    using A::A;

    template <typename U>
    void construct(U* ptr) noexcept(std::is_nothrow_default_constructible<U>::value) {
        ::new (static_cast<void*>(ptr)) U;
    }
    template <typename U, typename... Args> void construct(U* ptr, Args&&... args) {
        a_t::construct(static_cast<A&>(*this), ptr, std::forward<Args>(args)...);
    }
};

/* a std::vector<T> that does not initialise new members when doing resize(...) */
template <typename T> using vector_no_alloc = std::vector<T, default_init_allocator<T>>;

/****************** size ******************/
template <typename DataT, typename RangeT>
    requires(std::ranges::range<RangeT> &&
             std::is_same_v<DataT, std::ranges::range_value_t<RangeT>>)
size_t flatenned_size(RangeT&& buffers) {
    return std::size(buffers);
}

template <typename DataT, typename RangeRangeT, typename RangeT = std::ranges::range_value_t<RangeRangeT>>
    requires(std::ranges::range<RangeRangeT> && std::ranges::range<RangeT>
             && ! std::is_same_v<DataT,RangeT>)
size_t flatenned_size(RangeRangeT&& buffers) {
    size_t size = 0;
    for (const auto& buf : buffers)
        size += flatenned_size<DataT>(buf);
    return size;
}

/****************** flatten to vector ******************/

template <typename DataT, typename RangeT>
    requires(std::ranges::range<RangeT>)
std::vector<DataT> flatten(RangeT&& buffers) {
    auto size = flatenned_size<DataT>(buffers);

    std::vector<DataT> result(size);
    auto iterator = result.begin();
    sg::internal::flatten_copy_to_iterator(iterator, buffers);

    return result;
}

} // namespace sg::vector
