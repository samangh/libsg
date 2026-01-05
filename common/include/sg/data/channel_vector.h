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
    vector_channel(std::initializer_list<T> init):m_data(init){};
    vector_channel(std::string name) :m_name(std::move(name)) {}

    void from_bytes(const void* data, size_t byteCount) override {
        if (byteCount % sizeof(T) != 0)
            throw std::runtime_error("given set of bytes does not match the size of  teh channel type");

        m_data = std::vector<T>(static_cast<const T*>(data),
                                static_cast<const T*>(data) + (byteCount / sizeof(T)));
    }

    void reserve(size_t size) { m_data.reserve(size); }

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

    template <typename... Args>
    void emplace_back(Args&&... args) {
        m_data.emplace_back(std::forward<Args>(args)...);
    }

    template <typename TInput>
    void append(TInput&& input) {
        sg::ranges::append(m_data, std::forward<TInput>(input));
    }

    template <typename InputIt>
    void append(InputIt&& start, InputIt&& end) {
        m_data.insert(m_data.end(), std::forward<InputIt>(start), std::forward<InputIt>(end));
    }
};

typedef sg::data::vector_channel<double> t_chan_double_vec;

} // namespace sg::data
