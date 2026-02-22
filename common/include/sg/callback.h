#pragma once

#include <concepts>
#include <functional>

/** Creates a class named `NAME` representing a callback function that results `RESULT_T` and
 * arguments `...`. Use of these macro prevents two callbacks, which have the same call signature,
 * but represent different things, from being mistakenly passed.
 *
 * To call the callback directly, run the .invoke() member function.
 *
 * @code
 * CREATE_CALLBACK(callback1_t, void, int, int)
 *
 * callback1_t test = [](int, int) {...};
 * test.invoke(1,2);
 * @endcode
 *
 * @param NAME name of callback class to create
 * @param RESULT_TYPE result type of the callback
 * @param ... argument input signature for the callback function
 */
#define CREATE_CALLBACK(NAME, RESULT_TYPE, ...)                                                    \
    struct NAME {                                                                                  \
        typedef std::function<RESULT_TYPE(__VA_ARGS__)> func_t;                                    \
                                                                                                   \
        NAME() = default;                                                                          \
        NAME(std::nullptr_t) {};                                                                   \
                                                                                                   \
        /* Because of this, we can't define an operator() overload. If we did, all callback types  \
         * would satisfy the std::convertible_to concept and this whole class would become         \
         * useless! */                                                                             \
        template <typename InvokableT>                                                             \
            requires std::convertible_to<InvokableT, func_t>                                       \
        NAME(InvokableT func) : m_func(std::move(func)) {}                                         \
                                                                                                   \
        operator bool() const noexcept { return m_func != nullptr; }                               \
                                                                                                   \
        template <typename... ArgsT>                                                               \
        RESULT_TYPE invoke(ArgsT&&... args) {                                                      \
            return std::invoke(m_func, std::forward<ArgsT>(args)...);                              \
        }                                                                                          \
                                                                                                   \
      private:                                                                                     \
        func_t m_func{nullptr};                                                                    \
    };