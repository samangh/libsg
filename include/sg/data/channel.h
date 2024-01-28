#pragma once

#include <map>
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
};

template <typename T> class IChannel : public IChannelBase {
  public:
    virtual T *data() noexcept = 0;
    virtual const T* const_data() const noexcept = 0;
    virtual T operator[](int i) const = 0;
    virtual T &operator[](int i) = 0;
};

} // namespace sg::data
