#pragma once

#include "channel.h"
#include "sg/rolling_contiguous_buffer.h"

namespace sg::data {

template <typename T> class channel_rolling : public IContigiousChannel<T> {
    sg::rolling_contiguous_buffer<T> m_buff;

  public:
    channel_rolling(std::string name, size_t size) : IContigiousChannel<T>(name), m_buff(size) {}

    // IChannelBase interface
  public:

    size_t byte_size() const noexcept override {
        return m_buff.size() * sizeof(T);
    };

    size_t item_count() const noexcept override {
        return m_buff.size();
    };

    // IChannel interface
  public:
    const T *get() const noexcept override {
        return m_buff.data();
    };

    T *get() noexcept override {
        return m_buff.data();
    };

    std::string back_as_string() const override { return fmt::to_string(m_buff.back()); }
    void push_back(const T& val) { m_buff.push_back(val); }
    void emplace_back(T&& item) { m_buff.emplace_back(std::move(item)); };
};

} // namespace sg::data
