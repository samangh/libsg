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
 *      @code catch(const sg::exceptions::net&) @endcode
 *
 *  To catch a specific error:
 *      @code catch(const sg::exceptions::net::time_out&) @endcode
 */
namespace sg::exceptions {

/** base exception common to all libsg exceptions */
class any : public std::runtime_error {
  protected:
    explicit any(const std::string& msg) : runtime_error(msg) {}
};

#define REGISTER_EXCEPTION_SUBSYSTEM(SUBSYSTEM_NAME)                                               \
    namespace SUBSYSTEM_NAME {                                                                     \
        class any : public ::sg::exceptions::any {                                                 \
            using ::sg::exceptions::any::any;                                                      \
        };                                                                                         \
    }

#define CREATE_SUBSYSTEM_EXCEPTION(SUBSYSTEM_NAME, EXCEPTION_NAME, DEFAULT_DESCRIPTION)            \
    namespace SUBSYSTEM_NAME {                                                                     \
        class EXCEPTION_NAME : public any {                                                        \
          public:                                                                                  \
            EXCEPTION_NAME() : any(DEFAULT_DESCRIPTION) {}                                         \
            explicit EXCEPTION_NAME(const std::string& msg) : any(msg) {}                          \
        };                                                                                         \
    }

REGISTER_EXCEPTION_SUBSYSTEM(net);
CREATE_SUBSYSTEM_EXCEPTION(net, time_out, "operation timed out");
CREATE_SUBSYSTEM_EXCEPTION(net, host_not_found, "host not found");
CREATE_SUBSYSTEM_EXCEPTION(net, network_unreachable, "network unreachable");
CREATE_SUBSYSTEM_EXCEPTION(net, other, "other error");

} // namespace sg::exceptions