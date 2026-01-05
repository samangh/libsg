#pragma once

#include <sg/buffer.h>
#include <sg/math.h>
#include <sg/uuid.h>

#include <string>

namespace sg::data {

class IChannelBase {
    sg::uuids::uuid m_uuid;
  public:
    IChannelBase() = default;
    IChannelBase(sg::uuids::uuid uuid): m_uuid(uuid) {}
    virtual ~IChannelBase() = default;

    [[nodiscard]] virtual std::string name() const noexcept = 0;
    virtual void name(std::string) noexcept = 0;

    [[nodiscard]] virtual std::vector<std::string> hierarchy() const noexcept = 0;
    virtual void hierarchy(std::vector<std::string>) noexcept = 0;

    [[nodiscard]] virtual size_t size_bytes() const noexcept = 0;
    [[nodiscard]] virtual size_t count() const noexcept = 0;
    [[nodiscard]] virtual bool empty() const noexcept = 0;

    [[nodiscard]] virtual sg::uuids::uuid uuid() const noexcept { return m_uuid; };

    [[nodiscard]] virtual bool operator==(const IChannelBase& o) const {
        return o.uuid() == uuid();
    }

    [[nodiscard]] virtual bool operator<(const IChannelBase& o) const {
        if (hierarchy() != o.hierarchy())
            return (hierarchy() < o.hierarchy());

        if (name() != o.name())
            return (name() < o.name());

        return (uuid() < o.uuid());
    };
};

class IContigiousChannelBase: public IChannelBase {
  public:
    virtual void from_bytes(const void*, size_t byteCount)=0;

    /* Return the stored pointer.*/
    [[nodiscard]] virtual const void *get() const noexcept = 0;
    [[nodiscard]] virtual void *get() noexcept = 0;
};

template <typename T> class IContigiousChannel : public virtual IContigiousChannelBase {
  public:
    typedef T                                value_type;
    typedef std::size_t                      size_type;
    typedef sg::contiguous_iterator<T>       iterator_type;
    typedef sg::contiguous_iterator<const T> const_iterator_type;
    typedef T&                               reference;
    typedef const T&                         const_reference;

    IContigiousChannel() =default;
    virtual ~IContigiousChannel() = default;

    /* Return the stored pointer.*/
    [[nodiscard]] const void* get() const noexcept override {return data();}
    [[nodiscard]] void* get() noexcept override {return data();}

    [[nodiscard]] virtual const T *data() const noexcept = 0;
    [[nodiscard]] virtual T *data() noexcept = 0;

    [[nodiscard]] size_t size_bytes() const noexcept override { return count() * sizeof(T); }
    [[nodiscard]] virtual bool empty() const noexcept override { return count() == 0; }

    /* iterators */
    [[nodiscard]] iterator_type begin() { return iterator_type(data()); }
    [[nodiscard]] iterator_type end() { return begin() + count(); }

    /* const interators */
    [[nodiscard]] const_iterator_type begin() const { return const_iterator_type(data()); }
    [[nodiscard]] const_iterator_type end() const { return begin() + count(); }
    [[nodiscard]] const_iterator_type cbegin() const { return begin(); }
    [[nodiscard]] const_iterator_type cend() const { return end(); }

    /* front/back */
    [[nodiscard]] reference front() { return *begin(); }
    [[nodiscard]] reference back() { return *(end() - 1); }

    /* const front/back */
    [[nodiscard]] const_reference front() const {return *begin();}
    [[nodiscard]] const_reference back() const {return *(end() - 1);}

    [[nodiscard]] reference       operator[](size_t i) { return data()[i]; };
    [[nodiscard]] const_reference operator[](size_t i) const { return data()[i]; };

    [[nodiscard]] reference at(size_t i){ return (*this)[i];}
    [[nodiscard]] const_reference at(size_t i) const{ return (*this)[i];}
};

typedef sg::data::IContigiousChannel<double> t_chan_double;

} // namespace sg::data
