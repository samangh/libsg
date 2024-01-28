#pragma once

#include "channel.h"
#include "sg/compression_zstd.h"
#include "view.h"

#include <thread>

namespace sg::data {

template <typename T> class compressed_channel : public IChannel<T> {
    static inline const int DEFAULT_COMP_THREAD_COUNT = std::thread::hardware_concurrency();
    static inline constexpr int DEFAULT_COMPRESSION_LEVEl = 3;

    std::string m_name;
    std::vector<std::string> m_hierarchy;
    size_t m_size = 0;
    sg::unique_c_buffer<T> m_raw_buffer;
    sg::unique_c_buffer<uint8_t> m_compressed_data;
    std::weak_ptr<view<T>> m_weakptr;

    int m_cLevel = DEFAULT_COMPRESSION_LEVEl;
    int m_noThread = DEFAULT_COMP_THREAD_COUNT;

    void uncompress_data() {
        if (!m_compressed_data.get() || m_compressed_data.size() == 0)
            return;

        m_raw_buffer = sg::compression::zstd::decompress<T>(m_compressed_data);
        m_compressed_data.reset();
    }
    void compress_data() {
        if (!m_raw_buffer.get() || m_raw_buffer.size() == 0)
            return;

        m_compressed_data = sg::compression::zstd::compress(m_raw_buffer, m_cLevel, m_noThread);
        m_raw_buffer.reset();
    }

  public:
    compressed_channel() = default;
    compressed_channel(std::string name, std::vector<std::string> hierarchy) {
        this->name(name);
        this->hierarchy(hierarchy);
    }
    compressed_channel(std::string name,
                       std::vector<std::string> hierarchy,
                       sg::unique_c_buffer<T> &&inBuffer) {
        this->name(name);
        this->hierarchy(hierarchy);
        set_data(inBuffer);
    }

    void set_compression_level(int cLevel);
    void set_compresstion_thread_count(int noThread);

    void set_data(sg::unique_c_buffer<T> &&inBuffer) {
        m_size = inBuffer.size();
        m_raw_buffer = std::move(inBuffer);
        compress_data();
    }

    std::shared_ptr<view<T>> get_data_view() {
        /* If ther is already a shared_ptr, return that */
        auto ptr = m_weakptr.lock();
        if (ptr)
            return ptr;

        uncompress_data();

        ptr = std::shared_ptr<view<T>>(new view<T>(this), [this](view<T> *p) {
            this->compress_data();
            delete p;
        });
        m_weakptr = ptr;

        return m_weakptr.lock();
    }

    // IChannel interface
  public:
    T operator[](int i) const { return const_data()[i]; };
    T &operator[](int i) { return data()[i]; };

    std::string name() const noexcept { return m_name; }
    void name(std::string name) noexcept { m_name = name; }

    std::vector<std::string> hierarchy() const noexcept { return m_hierarchy; }
    void hierarchy(std::vector<std::string> hierarchy) noexcept { m_hierarchy = hierarchy; }
    size_t size() const noexcept { return m_size; };

    T *data() noexcept {return m_raw_buffer.get();};
    const T *const_data() const noexcept {return m_raw_buffer.get();};
};

} // namespace sg::data
