#pragma once

#include <concepts>
#include <functional>

namespace sg {

/** A type-tagged wrapper around `std::function<Signature>`.
 *
 * The `Tag` parameter exists solely to give two otherwise-identical callback
 * types distinct identities, so that two callbacks with the same call
 * signature representing different things can't be mistakenly passed for one
 * another. Use the @ref CREATE_CALLBACK macro to declare one (it generates
 * the tag automatically), or write the alias by hand:
 *
 * @code
 * using my_cb_t = sg::callback<struct my_cb_tag, void(int, int)>;
 * @endcode
 *
 * Note: `operator()` is deliberately omitted. If it existed, every callback
 * type would satisfy `std::convertible_to<func_t>` and the tag-based type
 * distinction would collapse. Use `.invoke(...)` instead.
 *
 * Note: having a undefined-base and then specialisation allows use to pass in a std::function-type
 * definition for the callback signature (e.g. void(int, int))
 */
template <typename Tag, typename Signature>
class callback;

template <typename Tag, typename ReturnT, typename... ArgsT>
class callback<Tag, ReturnT(ArgsT...)> {
  public:
    using func_t = std::function<ReturnT(ArgsT...)>;

    callback() = default;
    callback(std::nullptr_t) noexcept {}

    template <typename F>
        requires std::convertible_to<F, func_t>
    callback(F f) : m_func(std::move(f)) {}

    explicit operator bool() const noexcept { return m_func != nullptr; }

    //note: needs to be a template function for perfect forwarding to work in all cases
    template <typename... Args>
    constexpr ReturnT invoke(Args&&... args) const {
        return std::invoke(m_func, std::forward<Args>(args)...);
    }

  private:
    func_t m_func{nullptr};
};

}  // namespace sg

/** Declares a tagged callback type alias. Expands to a `using` declaration
 * over @ref sg::callback with an inline-declared phantom tag struct, so each
 * use produces a distinct C++ type.
 *
 * @code
 * CREATE_CALLBACK(callback1_t, void(int, int))
 *
 * callback1_t test = [](int, int) { ... };
 * test.invoke(1, 2);
 * @endcode
 *
 * @param NAME        name of the callback alias to create
 * @param SIGNATURE   signature the callback
 */
#define CREATE_CALLBACK(NAME, SIGNATURE) \
    using NAME = ::sg::callback<struct NAME##_tag, SIGNATURE>;
