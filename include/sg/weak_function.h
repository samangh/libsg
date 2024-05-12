#pragma once

#include "memory.h"

#include <functional>
#include <memory>

namespace sg {

/**
 * @brief A std::funnction tyype object, where the function will only be run if the
 * provided weak_ptr is valid.
 *
 * This is handly when passing a std::fucntion as a callback, and we want the
 * callback to not be called if the owning object is out of scope.
 *
 * @tparam T    type of oject held by the weakptr
 * @tparam R    return type of the function
 * @tparam ArgT types of function arguments
 */
template <typename T, typename R, typename... ArgT> class weak_function {
    std::weak_ptr<T> m_item;
    std::function<R(ArgT...)> m_func;

  public:
    weak_function(const std::nullptr_t &){}; // Allow cast from nullptr
    weak_function(std::weak_ptr<T> weakPtr, const std::function<R(ArgT...)>& func)
        : m_item(weakPtr),
          m_func(func){};

    R operator()(ArgT... args) {
        if (m_item.lock())
            return m_func(std::forward<ArgT>(args)...);

        return R();
    };
    virtual ~weak_function() = default;
};

template <typename R, typename... ArgTypes>
auto create_weak_function(sg::enable_lifetime_indicator *lifeimeIndicator,
                          const std::function<R(ArgTypes...)>& func) {
    return sg::weak_function(lifeimeIndicator->get_lifetime_indicator(), func);
}
} // namespace sg
