#pragma once

#include "channel.h"
#include "sg/rolling_contiguous_buffer.h"

namespace sg::data {

template <typename T> class channel_rolling : public IContigiousChannel<T> {
    sg::rolling_contiguous_buffer<T> m_data;

    std::string m_name;
    std::vector<std::string> m_hierarchy;
  public:
    channel_rolling() =default;
    channel_rolling(std::string name, size_t size) :m_name(name), m_data(size) {}

    void reserve(size_t size) { m_data.reserve(size); }
    virtual ~channel_rolling() = default;

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

} // namespace sg::data
