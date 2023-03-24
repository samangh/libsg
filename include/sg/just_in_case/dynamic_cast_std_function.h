#pragma once

#include <utility>
#include <type_traits>
#include <functional>

/* Taken from:
 * https://stackoverflow.com/questions/42439152/automatic-dynamic-cast-for-arguments-in-stdfunction
 *
 * Implements automatic dynamic_cast for arguments in std::function. As an example:
 *
 *
 * struct A {
 *   virtual ~A() {}
 * };
 *
 * struct B final : public A {
 *     void f() { std::cout << "f" << std::endl; }
 * };
 *
 * int main() {
 *     std::function<void(A*)> funcA = dynamic_cast_std_function([](B* b) { b->f(); });
 *     B b;
 *     funcA(&b);
 * }
 *
 */

namespace sg {

// C++17's void_t
template <class...>
using void_t = void;

// Pack of arbitrary types
template <class...>
struct pack { };

namespace dynamic_cast_std_function_detail_parameters {
        template <class F, class = void_t<>>
        struct parameters { };

        template <class F>
        struct parameters<F, void_t<decltype(&F::operator ())>> : parameters<decltype(&F::operator ())> { };

        template <class R, class... Params>
        struct parameters<R(Params...)> { using type = pack<Params...>; };

        template <class R, class... Params>
        struct parameters<R(*)(Params...)> { using type = pack<Params...>; };

        template <class T, class R, class... Params>
        struct parameters<R(T::*)(Params...)> { using type = pack<Params...>; };

        template <class T, class R, class... Params>
        struct parameters<R(T::*)(Params...) const> { using type = pack<Params...>; };
}

// Retrieve the parameter list from a functionoid
template <class F>
using parameters = typename dynamic_cast_std_function_detail_parameters::parameters<std::remove_reference_t<F>>::type;

/** @brief dynamic cast for argumnets of std::function<> */
template <class F, class... Ps>
auto dynamic_cast_std_function(F &&f, pack<Ps...>) {
    return [f_ = std::forward<F>(f)](auto *... args) -> decltype(auto) {
        return f_(dynamic_cast<Ps>(args)...);
    };
}

/** @brief dynamic cast for argumnets of std::function<> */
template <class F>
auto dynamic_cast_std_function(F &&f) {
    return dynamic_cast_std_function(std::forward<F>(f), parameters<F>{});
}


}
