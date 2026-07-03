#pragma once

#include "export.h"

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
 *      @code catch(const sg::exceptions::net::any&) @endcode
 *
 *  To catch a specific error:
 *      @code catch(const sg::exceptions::net::time_out&) @endcode
 *
 *  Since the default viability is set to hidden, note the comments about
 *  exception classes in https://gcc.gnu.org/wiki/Visibility. Exception classes
 *  MUST have be exported (i.e. visibility set to "default")
  */
namespace sg::exceptions {


/** base exception common to all libsg exceptions */
class VISIBLE any : public std::runtime_error {
  protected:
    explicit any(const std::string& msg) : runtime_error(msg) {}
};

/**
 * Creates the top level 'any' exception in the current namespace
 */
#define SG_CREATE_EXCEPTION_ANY()                                                                  \
    class VISIBLE any : public ::sg::exceptions::any {                                             \
        using ::sg::exceptions::any::any;                                                          \
    };

/**
 * Defines a custom exception in the current namespace.
 *
 * The generated class provides a default constructor that uses
 * @p DEFAULT_DESCRIPTION as the error message, and an explicit constructor
 * accepting a custom message.
 *
 * Note that you must have called SG_CREATE_EXCEPTION_ANY() before calling this macro.
 *
 * @param EXCEPTION_NAME       Name of the exception class to create.
 * @param DEFAULT_DESCRIPTION  String literal used as the default error message.
 */
#define SG_CREATE_EXCEPTION(EXCEPTION_NAME, DEFAULT_DESCRIPTION)                                   \
    class VISIBLE EXCEPTION_NAME : public any {                                                    \
      public:                                                                                      \
        EXCEPTION_NAME() : any(DEFAULT_DESCRIPTION) {}                                             \
        explicit EXCEPTION_NAME(const std::string& msg) : any(msg) {}                              \
    };
} // namespace sg::exceptions
