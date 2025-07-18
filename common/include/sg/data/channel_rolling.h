#pragma once

#include "channel.h"
#include "sg/rolling_contiguous_buffer.h"

namespace sg::data {

template <typename T> class channel_rolling : public IContigiousChannel<T> {
    sg::rolling_contiguous_buffer<T> m_data;

    std::string m_name;
    std::vector<std::string> m_hierarchy;
  public:
    channel_rolling() = default;
    channel_rolling(std::string name, size_t size, size_t reservereSize)
        : m_data(size, reservereSize),
          m_name(name) {}
    channel_rolling(std::string name, size_t size) : channel_rolling(name, size, size) {}

    void reserve(size_t size, double reserverFactor = 1.0) { m_data.reserve(size, reserverFactor); }
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

    void append(std::initializer_list<T> ilist) {
        m_data.append(std::move(ilist));
    }

    template <typename RangeT>
    void append(RangeT&& to_add) {
        m_data.append(std::forward<RangeT>(to_add));
    }

    template <typename InputIt> void append(InputIt&& start, InputIt&& end) {
        m_data.append(std::forward<InputIt>(start), std::forward<InputIt>(end));
    }

};

} // namespace sg::data
