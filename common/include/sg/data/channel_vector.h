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
    size_t count() const noexcept override {
        return m_data.size();
    };

    T *data() noexcept override {
        return m_data.data();
    };

    std::string back_as_string() const { return sg::math::to_string(m_data.back()); };

    void clear() { m_data.clear(); };
    void push_back(const T& item) { m_data.push_back(item); };

    template <typename... Args> void emplace_back(Args&&... args) {
        m_data.emplace_back(std::forward<Args>(args)...);
    };
};

typedef sg::data::vector_channel<double> t_chan_double_vec;

} // namespace sg::data
