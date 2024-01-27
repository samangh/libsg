#pragma once

#include "data_source.h"
#include "data_view.h"
#include "sg/compression_zstd.h"

namespace sg::data {

template <typename T> class compressed_data_source : public ISource<T> {
    std::string m_name;
    size_t m_size = 0;
    sg::unique_c_buffer<T> m_raw_buffer;
    sg::unique_c_buffer<uint8_t> m_compressed_data;
    std::weak_ptr<data_view<T>> m_weakptr;

    void uncompress_data() {
        if (!m_compressed_data.get() || m_compressed_data.size() == 0)
            return;

        m_raw_buffer = sg::compression::zstd::decompress<T>(m_compressed_data);
        m_compressed_data.reset();
    }
    void compress_data() {
        if (!m_raw_buffer.get() || m_raw_buffer.size()==0)
            return;

        m_compressed_data = sg::compression::zstd::compress(m_raw_buffer, 0, 4);
        m_raw_buffer.reset();
    }
 public:
    std::string name() const noexcept { return m_name; }
    void name(const std::string &name) noexcept { m_name = name; }

    size_t size() const noexcept { return m_size; };
    void set_data(sg::unique_c_buffer<T> &&inBuffer) {
        m_size = inBuffer.size();
        m_raw_buffer = std::move(inBuffer);
        compress_data();
    }
    T *data() const noexcept { return m_raw_buffer.get(); }

    std::shared_ptr<data_view<T>> get_data_view() {
        /* If ther is already a shared_ptr, return that */
        auto ptr = m_weakptr.lock();
        if (ptr)
            return ptr;

        uncompress_data();

        ptr = std::shared_ptr<data_view<T>>(new data_view<T>(this), [this](data_view<T> *p) {
            this->compress_data();
            delete p;
        });
        m_weakptr = ptr;

        return m_weakptr.lock();
    }
};

} // namespace sg::data
