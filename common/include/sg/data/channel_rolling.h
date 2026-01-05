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
    channel_rolling(std::string name, size_t size, size_t reserverSize)
        : m_data(size, reserverSize),
          m_name(std::move(name)) {}
    channel_rolling(std::string name, size_t size) : channel_rolling(name, size, size) {}

    void from_bytes(const void* data, size_t byteCount) override {
        if (byteCount % sizeof(T) != 0)
            throw std::runtime_error("given set of bytes does not match the size of  teh channel type");
        append(static_cast<const T*>(data), static_cast<const T*>(data) + (byteCount / sizeof(T)));
    }

    void reserve(size_t size, double reserverFactor = 1.0) { m_data.reserve(size, reserverFactor); }

  public:
    [[nodiscard]] std::string name() const noexcept override { return m_name; }
    void name(std::string name) noexcept override { m_name = name; }

    [[nodiscard]] std::vector<std::string> hierarchy() const noexcept override {
        return m_hierarchy;
    }
    void hierarchy(std::vector<std::string> hierarchy) noexcept override {
        m_hierarchy = std::move(hierarchy);
    }

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
