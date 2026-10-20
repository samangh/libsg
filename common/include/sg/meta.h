#pragma once

#include <string_view>

namespace sg::meta {

/** Returns the name of @c T as a string.
 *
 * Use decltype() on a variable to name its type; `sg::meta::type_name<decltype(i)>()` returns "int"
 * if @c i is an int.
 *
 * The name comes from the compiler's own description of this function, so its exact spelling is
 * not portable: MSVC writes "class std::vector<int,...>" where GCC and clang write
 * "std::vector<int>". Treat the result as diagnostic text, not as something to parse or compare.
 *
 * Unsupported compilers return "Error: unsupported compiler" rather than failing to compile.
 */
template <typename T> [[nodiscard]] constexpr std::string_view type_name() noexcept {
    /* This function must be in the header: it reads the signature of its own instantiation.
     *
     * Based on
     * https://stackoverflow.com/questions/81870/is-it-possible-to-print-a-variables-type-in-standard-c/56766138#56766138
     * but searching for the delimiters instead of assuming a fixed prefix, so that renaming or
     * moving this function, or changing its return type, cannot silently truncate the name. */
#if defined(__clang__) || defined(__GNUC__)
    /* "... [T = int]" on clang, "... [with T = int; ...]" on GCC. */
    constexpr std::string_view signature = __PRETTY_FUNCTION__;
    constexpr std::string_view opening = "T = ";

    const auto start = signature.find(opening);
    if (start == std::string_view::npos)
        return "Error: unsupported compiler";

    const auto name = signature.substr(start + opening.size());
    return name.substr(0, name.find_first_of(";]"));
#elif defined(_MSC_VER)
    /* "... sg::meta::type_name<int>(void) noexcept", so the last '>' closes the argument list. */
    constexpr std::string_view signature = __FUNCSIG__;
    constexpr std::string_view opening = "type_name<";

    const auto start = signature.find(opening);
    if (start == std::string_view::npos)
        return "Error: unsupported compiler";

    const auto name = signature.substr(start + opening.size());
    return name.substr(0, name.rfind('>'));
#else
    return "Error: unsupported compiler";
#endif
}

} // namespace sg::meta
