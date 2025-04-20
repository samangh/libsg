#pragma once

#include "channel.h"

namespace sg::data {

template <typename T> class vector_channel : public IContigiousChannel<T> {
    std::vector<T> m_data;
    typedef IContigiousChannel<T> parent;
  public:
    void reserve(size_t size) { m_data.reserve(size); }
    virtual ~vector_channel() = default;

  public:
    size_t size_bytes() const noexcept override {
        return m_data.size() * sizeof(T);
    };

    size_t count() const noexcept override {
        return m_data.size();
    };

    const T *data() const noexcept override {
        return m_data.data();
    };

    T *data() noexcept override {
        return m_data.data();
    };

    std::string back_as_string() const { return sg::math::to_string(m_data.back()); };

    void clear() { m_data.clear(); };
    void push_back(const T& item) { m_data.push_back(item); };
    void emplace_back(T&& item) { m_data.emplace_back(std::move(item)); };
};

typedef sg::data::vector_channel<double> t_chan_double_vec;

} // namespace sg::data
