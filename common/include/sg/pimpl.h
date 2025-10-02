#pragma once

#include <memory>
#if __has_include(<experimental/propagate_const>)
    #include <experimental/propagate_const>
#else
    #include "internal/propagate_const.h"
#endif

namespace sg {

/**
 * @brief   Implements pimpl algorithm.
 *
 * Use like follows:
 *   \code{.cpp}
 *      class public_facing_class {
 *        struct impl;
 *        sg::pimpl<impl> pimpl;
 *
 *        ~public_facing_class();
 *      }
 *   \endcode
 *
 * Note that the destructor must be in the implementation, as the
 * destructor needs to know the size of impl
 *
 * @tparam  T   the impl object type
 */
template <typename T> class pimpl {
    /**
     * @brief The implementation pointer, wrapped in propagate_const.
     *
     * This is useful, otherwise the compiler will not catch changes
     * to member variables of #imp within const member functions.
     *
     * When the pimpl object is wrapped in propagate_const, const member
     * unctions will only be able to call const functions on the pimpl
     * object and will be unable to modify (non-mutable) member variables
     * of the pimpl object without explicit const_cast.
     *
     * Taken from Herb Sutter and
     * https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4388.html
     */
    std::experimental::propagate_const<std::unique_ptr<T>> impl_ptr;

  public:
    pimpl() : impl_ptr(std::make_unique<T>()) {}

    template <typename... Args>
    pimpl(Args &&...args) : impl_ptr(std::make_unique<T>(std::forward<Args>(args)...)) {}

    T *operator->() { return impl_ptr.get(); }
    T &operator*() { return *impl_ptr.get(); }

    const std::experimental::propagate_const<std::unique_ptr<T>> &operator->() const {
        return impl_ptr;
    }
};

} // namespace sg
