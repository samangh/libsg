#pragma once

#include <sg/buffer.h>
#include <sg/math.h>

#include <string>

namespace sg::data {

class IChannelBase {
  public:
    virtual ~IChannelBase() = default;

    [[nodiscard]] virtual std::string name() const noexcept = 0;
    virtual void name(std::string) noexcept = 0;

    [[nodiscard]] virtual std::vector<std::string> hierarchy() const noexcept = 0;
    virtual void hierarchy(std::vector<std::string>) noexcept = 0;

    [[nodiscard]] virtual size_t byte_size() const noexcept = 0;
    [[nodiscard]] virtual size_t item_count() const noexcept = 0;

    [[nodiscard]] virtual bool operator==(const IChannelBase &o) const {
        //TODO NEED UUID or GUID
        return name() == o.name() && hierarchy() == o.hierarchy() && byte_size() == o.byte_size();
    }
    [[nodiscard]] virtual bool operator<(const IChannelBase &o) const {
        //TODO NEED UUID or GUID
        if (hierarchy() != o.hierarchy())
            return (hierarchy() < o.hierarchy());

        if (name() != o.name())
            return (name() < o.name());

        if (byte_size() != o.byte_size())
            return (byte_size() < o.byte_size());

        return false;
    };
};

template <typename T> class IContigiousChannel : public IChannelBase {
    std::string m_name;
    std::vector<std::string> m_hierarchy;

    typedef T                                value_type;
    typedef std::size_t                      size_type;
    typedef sg::contiguous_iterator<T>       iterator_type;
    typedef sg::contiguous_iterator<const T> const_iterator_type;
    typedef T&                               reference;
    typedef const T&                         const_reference;

  public:
    IContigiousChannel() =default;
    IContigiousChannel(std::string name) :m_name(name) {}

    virtual ~IContigiousChannel() = default;

    std::string name() const noexcept { return m_name; }
    void name(std::string name) noexcept { m_name = name; }

    std::vector<std::string> hierarchy() const noexcept { return m_hierarchy; }
    void hierarchy(std::vector<std::string> hierarchy) noexcept { m_hierarchy = hierarchy; }

    /* Return the stored pointer.*/
    [[nodiscard]] virtual const T *get() const noexcept = 0;
    [[nodiscard]] virtual T *get() noexcept = 0;

    /* iterators */
    [[nodiscard]] constexpr iterator_type begin() { return iterator_type(get()); }
    [[nodiscard]] constexpr iterator_type end() { return begin() + byte_size(); }

    /* const interators */
    [[nodiscard]] constexpr const_iterator_type begin() const { return const_iterator_type(get()); }
    [[nodiscard]] constexpr const_iterator_type end() const { return begin() + byte_size(); }
    [[nodiscard]] constexpr const_iterator_type cbegin() const { return begin(); }
    [[nodiscard]] constexpr const_iterator_type cend() const { return end(); }

    /* front/back */
    [[nodiscard]] reference front() { return *begin(); }
    [[nodiscard]] reference back() { return *(end() - 1); }

    /* const front/back */
    [[nodiscard]] const_reference front() const {return *begin();}
    [[nodiscard]] const_reference back() const {return *(end() - 1);;}

    [[nodiscard]] T&       operator[](size_t i) { return get()[i]; };
    [[nodiscard]] const T& operator[](size_t i) const { return get()[i]; };
};

typedef sg::data::IContigiousChannel<double> t_chan_double;

} // namespace sg::data
