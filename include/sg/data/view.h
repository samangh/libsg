#pragma once

#include "channel.h"

namespace sg::data {

template <typename T> class IView {
  public:
    virtual std::string name() const noexcept = 0;
    virtual size_t count() const noexcept = 0;
    virtual T *data() noexcept = 0;
    virtual const T *data() const noexcept = 0;
};

template <typename T> class view_compressed : public IView<T> {
    const IChannel<T>& m_channel;
    T* m_data;
  public:
    view_compressed(const IChannel<T>& src, T* data) :m_channel(src), m_data(data) { }

    std::string name() const noexcept { return m_channel.name(); }
    size_t count() const noexcept { return m_channel.size(); }

    T operator[](int i) const { return m_data[i]; };
    T &operator[](int i) { return m_data[i]; };
    T *data() noexcept { return m_data; }
    const T *data() const noexcept { return m_data; }
};

} // namespace sg::data
