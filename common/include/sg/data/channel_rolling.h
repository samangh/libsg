#pragma once

#include "channel.h"
#include "sg/rolling_contiguous_buffer.h"

namespace sg::data {

template <typename T> class channel_rolling : public IChannel<T> {
    sg::rolling_contiguous_buffer<T> m_buff;

  public:
    channel_rolling(std::string name, size_t size) : IChannel<T>(name), m_buff(size) {}

    // IChannelBase interface
  public:
    size_t size() const noexcept override { return m_buff.size(); }

    // IChannel interface
  public:
    T*       data() noexcept override { return m_buff.data(); }
    const T* data() const noexcept override { return m_buff.data(); }

    const T& back() const override { return m_buff.back(); }
    T&       back() override { return m_buff.back(); }
    T&       front() override { return m_buff.front(); }
    const T& front() const override { return m_buff.front(); }

    std::string back_as_string() const override { return fmt::to_string(back()); }
    const T&    operator[](size_t i) const override { return m_buff[i]; }
    T&          operator[](size_t i) override { return m_buff[i]; }

    void push_back(T val) { m_buff.push_back(val); }
};

} // namespace sg::data
