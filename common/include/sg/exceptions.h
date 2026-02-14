#pragma once

#include <stdexcept>

namespace sg::exceptions {

/** base exception common to all libsg exceptions */
class exception_base : public std::runtime_error {
  protected:
    explicit exception_base(std::string msg) : runtime_error(std::move(msg)) {}
};

/** exception for specific libsg subsystems */
template <typename ErrorT>
class exception : public exception_base {
  public:
    typedef ErrorT codes;

    explicit exception(ErrorT errr_code, std::string msg)
        : exception_base(std::move(msg)),
          code(errr_code) {}
    const ErrorT code;
};

/* define "subsystem" specific errors */
enum class errors_net { network_unreachable, host_not_found, other };

/* subsystem specific exceptions */
typedef exception<errors_net> net;

} // namespace sg::exceptions