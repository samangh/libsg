#pragma once

#include <vector>
#include <ranges>

namespace sg::vector {

template <typename T, typename RangeT>
requires(std::ranges::range<RangeT> &&
         std::is_same_v<std::ranges::range_value_t<RangeT>,T>)
void append(std::vector<T> &base, const RangeT& to_add) {
    base.insert(std::end(base), std::begin(to_add), std::end(to_add));
}

template <typename T, typename RangeT>
requires(std::ranges::range<RangeT> &&
         std::is_same_v<std::ranges::range_value_t<RangeT>,T>)
void append(std::vector<T> &base, RangeT&& to_add) {
    base.insert(std::end(base), make_move_iterator(std::begin(to_add)), make_move_iterator(std::end(to_add)));
}

/**
 * @brief Move contents of one vector to the end of another.
 *
 * There is no point in using this for basic types where a move is
 * equivalent to copy (e.g. ini, double). But for more complex types
 * this is useful.
 */
template <typename T> void append(std::vector<T> &base, std::vector<T> &&to_add) {
    base.insert(
        std::end(base), make_move_iterator(to_add.begin()), make_move_iterator(to_add.end()));
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
template <typename T> using vector_no_initialise = std::vector<T, default_init_allocator<T>>;

} // namespace sg::vector
