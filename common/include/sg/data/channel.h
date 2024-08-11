#pragma once

#include <sg/buffer.h>
#include <sg/math.h>

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
        //TODO NEED UUID or GUID
        return name() == o.name() && hierarchy() == o.hierarchy() && size() == o.size();
    }
    virtual bool operator<(const IChannelBase &o) const {
        //TODO NEED UUID or GUID
        if (hierarchy() != o.hierarchy())
            return (hierarchy() < o.hierarchy());

        if (name() != o.name())
            return (name() < o.name());

        if (size() != o.size())
            return (size() < o.size());

        return false;
    };
    virtual ~IChannelBase() = default;
};

template <typename T> class IChannel : public IChannelBase {
    std::string m_name;
    std::vector<std::string> m_hierarchy;

  public:
    std::string name() const noexcept { return m_name; }
    void name(std::string name) noexcept { m_name = name; }

    std::vector<std::string> hierarchy() const noexcept { return m_hierarchy; }
    void hierarchy(std::vector<std::string> hierarchy) noexcept { m_hierarchy = hierarchy; }

    virtual T *data() noexcept = 0;
    virtual const T *data() const noexcept = 0;

    virtual T& back() = 0;
    virtual const T& back() const = 0;

    virtual T& front() = 0;
    virtual const T& front() const = 0;

    virtual std::string back_as_string() const = 0;

    virtual const T& operator[](size_t i) const = 0;
    virtual T& operator[](size_t i) = 0;
};

typedef sg::data::IChannel<double> t_chan_double;

} // namespace sg::data
