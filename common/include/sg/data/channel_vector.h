#pragma once

#include "channel.h"

namespace sg::data {

template <typename T> class vector_channel : public IChannel<T> {
    std::vector<T> m_data;

  public:
    void reserve(size_t size) { m_data.reserve(size); }

  public:
    size_t size() const noexcept override { return m_data.size(); }

    T*       data() noexcept override { return m_data.data(); }
    const T* data() const noexcept override { return m_data.data(); }

    const T& back() const override { return m_data.back(); }
    T&       back() override { return m_data.back(); }
    T&       front() override { return m_data.front(); }
    const T& front() const override { return m_data.front(); }

    const T& operator[](size_t i) const override { return m_data[i]; };
    T&       operator[](size_t i) override { return m_data[i]; }

    std::string back_as_string() const override { return sg::math::to_string(back()); };

    void clear() { m_data.clear(); };
    void push_back(const T& item) { m_data.push_back(item); };
    void push_back(T&& item) { m_data.emplace_back(std::move(item)); };
};

typedef sg::data::vector_channel<double> t_chan_double_vec;

} // namespace sg::data
