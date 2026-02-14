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
    enum class net { network_unreachable, host_not_found, other };
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
  protected:
    explicit exception(ErrorT errr_code, const std::string& msg) : any(msg), error(errr_code) {}
    const ErrorT error;
};

/** Create an exception class that can be used like NAME<ERR_TYPE>
 *
 * To catch:
 *   - specific error: use @code catch(const NAME<ERR_TYPE value>&) @endcode
 *   - any exception: use @code catch(const EXCEPTION_BASE<ERR_TYPE>&) @endcode
 *
 * @param NAME name of class to create
 * @param ERR_TYPE error type (this is usually an enum)
 * @param EXCEPTION_BASE exception class to use, (e.g. @code sg::exceptions::exception @endcode)
 */
#define CREATE_EXCEPTION_CLASS(NAME, ERR_TYPE, EXCEPTION_BASE)                                     \
    template <ERR_TYPE err>                                                                        \
    class NAME : public EXCEPTION_BASE<ERR_TYPE> {                                                 \
      public:                                                                                      \
        /* A variadic constructor can match the copy operator, so define it separately to prevent  \
         * problems. Otherwise, `throw exception(..)` will match the variadic constructor. See     \
         * https://stackoverflow.com/questions/77244527                                            \
         *                                                                                         \
         * clang/gcc will hide this because of copy elision when throwing, but MSVC won't. */      \
        NAME(NAME&) = default;                                                                     \
                                                                                                   \
        template <typename... ArgsT>                                                               \
        NAME(ArgsT&&... args) : EXCEPTION_BASE<ERR_TYPE>(err, std::forward<ArgsT>(args)...) {}     \
    };

CREATE_EXCEPTION_CLASS(net, errors::net, exception)



} // namespace sg::exceptions