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

namespace sg::ranges {

/**************************** append ****************************/
template <typename BaseRangeT, typename RangeT>
    requires(std::ranges::range<RangeT> && std::ranges::range<BaseRangeT> &&
        std::is_same_v<std::ranges::range_value_t<RangeT>,
                       std::ranges::range_value_t<BaseRangeT>>)
void append(BaseRangeT& baseRange, const RangeT& to_add) {
    baseRange.insert(std::end(baseRange), std::begin(to_add), std::end(to_add));
}

template <typename BaseRangeT, typename RangeT>
    requires(std::ranges::range<RangeT> && std::ranges::range<BaseRangeT> &&
             std::is_same_v<std::ranges::range_value_t<RangeT>,
                            std::ranges::range_value_t<BaseRangeT>> &&
             !std::is_lvalue_reference_v<RangeT>)
void append(BaseRangeT& baseRange, RangeT&& to_add) {
    baseRange.insert(std::end(baseRange),
                     make_move_iterator(std::begin(to_add)),
                     make_move_iterator(std::end(to_add)));
}

/**************************** copy ****************************/
template <typename InputRangeT, typename OutputIter>
OutputIter copy(InputRangeT to_add, OutputIter to_copy) {
    return std::copy(std::begin(to_add),
                     std::end(to_add), to_copy);
}

/**
 * @brief Allocator adaptor that interposes construct() calls to
 *        convert value initialization into default initialization.
 *
 *        Pass to std::vector, to create a vector that does not initialise
 *        the values.
 */
template <typename T, typename A = std::allocator<T>> class allocator_no_init : public A {
    /* From:
     * https://stackoverflow.com/questions/21028299/is-this-behavior-of-vectorresizesize-type-n-under-c11-and-boost-container/21028912
     */
    typedef std::allocator_traits<A> a_t;

  public:
    template <typename U> struct rebind {
        using other = allocator_no_init<U, typename a_t::template rebind_alloc<U>>;
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
template <typename T> using vector_no_init = std::vector<T, allocator_no_init<T>>;

/****************** size ******************/
template <typename DataT, typename RangeT>
    requires(std::ranges::range<RangeT> &&
             std::is_same_v<DataT, std::ranges::range_value_t<RangeT>>)
size_t flatenned_size(const RangeT& buffers) {
    return std::size(buffers);
}

template <typename DataT, typename RangeRangeT, typename RangeT = std::ranges::range_value_t<RangeRangeT>>
    requires(std::ranges::range<RangeRangeT> && std::ranges::range<RangeT>
             && ! std::is_same_v<DataT,RangeT>)
size_t flatenned_size(const RangeRangeT& buffers) {
    size_t size = 0;
    for (const auto& buf : buffers)
        size += flatenned_size<DataT>(buf);
    return size;
}

/****************** flatten to vector ******************/

template <typename DataT, typename Alloc = std::allocator<DataT>, typename RangeT>
    requires(std::ranges::range<RangeT> && std::derived_from<Alloc, std::allocator<DataT>>)
std::vector<DataT, Alloc> flatten(RangeT&& buffers) {
    auto size = flatenned_size<DataT>(buffers);

    std::vector<DataT, Alloc> result(size);
    auto iterator = result.begin();
    sg::internal::flatten_copy_to_iterator(iterator, buffers);

    return result;
}

template <typename DataT, typename RangeT>
    requires(std::ranges::range<RangeT>)
sg::ranges::vector_no_init<DataT> flatten_no_init(RangeT&& buffers) {
    return flatten<DataT,sg::ranges::allocator_no_init<DataT>>(buffers);
}

} // namespace sg::vector
