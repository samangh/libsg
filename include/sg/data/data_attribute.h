#pragma once

#include <cstdint>
#include <string>

namespace sg::data {


class metadata_manager{
public:
    virtual void set_int(const std::string name, int value);
    virtual void et_double(const std::string name, double value);
    virtual void set_string(const std::string name, const std::string & value);
    virtual void set_epoch(const std::string name, double value);

    virtual int as_int();
    virtual double as_double();
    virtual std::string as_string();
    virtual double as_epochtime();
};

/////////////////////////////////////////////

enum class metadata_type { Int, Double, String, EpochTime };

class meta {
public:
    virtual metadata_type value_type() const noexcept = 0;
    virtual int as_int() const = 0;
    virtual double as_double() const = 0;
    virtual std::string as_string() const = 0;
    virtual double as_epochtime() const = 0;
};

class metadata_base {
public:
    virtual metadata_type value_type() const noexcept = 0;
};

template <typename T> class metadata : public metadata_base {
    T value;
public:
    virtual T get() const noexcept =0;
    virtual void set(const T&) = 0;
};

} // namespace sg::data
