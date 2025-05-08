#pragma once

#include "channel.h"

namespace sg::data {

template <typename T> class vector_channel : public IContigiousChannel<T> {
    std::vector<T> m_data;
    typedef IContigiousChannel<T> parent;

    std::string m_name;
    std::vector<std::string> m_hierarchy;
  public:
    vector_channel() =default;
    vector_channel(std::string name) :m_name(name) {}

    void reserve(size_t size) { m_data.reserve(size); }
    virtual ~vector_channel() = default;

  public:
    [[nodiscard]] std::string name() const noexcept override { return m_name; }
    void name(std::string name) noexcept override { m_name = name; }

    [[nodiscard]] std::vector<std::string> hierarchy() const noexcept override { return m_hierarchy; }
    void hierarchy(std::vector<std::string> hierarchy) noexcept override { m_hierarchy = hierarchy; }

    [[nodiscard]] size_t count() const noexcept override { return m_data.size(); };

    [[nodiscard]] T* data() noexcept override { return m_data.data(); }
    [[nodiscard]] const T *data() const noexcept override { return m_data.data(); }

    void clear() { m_data.clear(); };
    void push_back(const T& item) { m_data.push_back(item); };
    template <typename... Args> void emplace_back(Args&&... args) {
        m_data.emplace_back(std::forward<Args>(args)...);
    };

    [[nodiscard]] std::string back_as_string() const override { return sg::math::to_string(m_data.back()); };

};

typedef sg::data::vector_channel<double> t_chan_double_vec;

} // namespace sg::data
