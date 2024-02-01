#pragma once

#include <sg/buffer.h>
#include <string>

namespace sg::data {

class IChannelBase {
  public:
    virtual std::string name() const noexcept = 0;
    virtual void name(std::string) noexcept = 0;

    virtual std::vector<std::string> hierarchy() const noexcept = 0;
    virtual void hierarchy(std::vector<std::string>) noexcept = 0;

    virtual size_t size() const noexcept = 0;

    virtual bool operator==(const IChannelBase &o) const {
        return name() == o.name() && hierarchy() == o.hierarchy() && size() == o.size() &&
               const_data_voidptr() == o.const_data_voidptr();
    }
    virtual bool operator<(const IChannelBase &o) const {
        if (hierarchy() != o.hierarchy())
            return (hierarchy() < o.hierarchy());

        if (name() != o.name())
            return (name() < o.name());

        if (size() != o.size())
            return (size() < o.size());

        if (const_data_voidptr() != o.const_data_voidptr())
            return (const_data_voidptr() < o.const_data_voidptr());

        return false;
    };

  protected:
    virtual const void *const_data_voidptr() const = 0;
};

template <typename T> class IChannel : public IChannelBase {
  public:
    virtual T *data() noexcept = 0;
    virtual const T* const_data() const noexcept = 0;
    virtual T operator[](int i) const = 0;
    virtual T &operator[](int i) = 0;
  protected:
    virtual const void *const_data_voidptr() const { return (void *)const_data(); };
};

} // namespace sg::data
