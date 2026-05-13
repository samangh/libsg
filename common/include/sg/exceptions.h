#pragma once

#include <string>
#include <stdexcept>
#include <utility>

/**
 *  Defines custom exception types
 *
 *  To catch all libsg exceptions:
 *      @code catch(const sg::exceptions::any&) @endcode
 *
 *  To catch all subsystem extensions:
 *      @code catch(const sg::exceptions::exception<sg::exceptions::errors::net>&) @endcode
 *
 *  To catch a specific error:
 *      @code catch(const sg::exceptions::net<sg::exceptions::errors::net::host_not_found>&) @endcode
 */
namespace sg::exceptions {

/* define "subsystem" specific errors */
namespace errors {
    enum class net { time_out, network_unreachable, host_not_found, other };
}

/** base exception common to all libsg exceptions */
class any : public std::runtime_error {
  protected:
    explicit any(const std::string& msg) : runtime_error(msg) {}
};

/** exception for specific libsg subsystems */
template <typename ErrorT>
    requires(std::is_enum_v<ErrorT>)
class exception : public any {
    ErrorT error;

  protected:
    explicit exception(ErrorT err_code, const std::string& msg) : any(msg), error(err_code) {}

  public:
    [[nodiscard]] ErrorT code() const noexcept { return error; }
};

/** Concrete exception parameterised by a specific error enum.
 *
 * Define new error classes using a templated <tt>using</tt>. For example, to add exceptions for the
 * <tt>net</tt> subsystem:
 * @code
 *     template <errors::net Err>
 *     using net = typed<Err>;
 * @endcode
 *
 * Then, for a timeout error do:
 *   - specific error: use @code catch(const net<errors::net::time_out>&) @endcode
 *   - any exception: use @code catch(const exception<errors::net>&) @endcode
 *
 * @tparam Error specific error enum value
 */
template <auto Error>
    requires(std::is_enum_v<decltype(Error)>)
class typed : public exception<decltype(Error)> {
  public:
    /* A variadic constructor can match the copy operator, so define it separately to prevent
     * problems. Otherwise, `throw exception(..)` will match the variadic constructor. See
     * https://stackoverflow.com/questions/77244527
     *
     * clang/gcc will hide this because of copy elision when throwing, but MSVC won't. */
    typed(typed&) = default;
    typed(const typed&) = default;

    template <typename... ArgsT>
    explicit typed(ArgsT&&... args)
        : exception<decltype(Error)>(Error, std::forward<ArgsT>(args)...) {}
};

#define CREATE_SUBSYSTEM_EXCEPTION(NAME, ENUM_TYPE)                                                \
    template <ENUM_TYPE Err>                                                                       \
    using NAME = typed<Err>;

CREATE_SUBSYSTEM_EXCEPTION(net, errors::net);

} // namespace sg::exceptions