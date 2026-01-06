#pragma once

#include "channel.h"
#include "sg/compression_zstd.h"

#include <concepts>

namespace sg::data {

template <typename TChannel, typename T = TChannel::value_type>
requires (std::derived_from<TChannel, IContigiousChannel<T>>)
class compressed_channel {
    struct deleter_free {
        constexpr void operator()(TChannel* p) const noexcept { free(p); }
    };public:
private:
    std::shared_ptr<TChannel> m_ch;
};

}

// #include "view.h"

// #include <thread>

// namespace sg::data {

// template <typename T> class compressed_channel : public IChannel<T> {
//     static inline const int DEFAULT_COMP_THREAD_COUNT = std::thread::hardware_concurrency();
//     static inline constexpr int DEFAULT_COMPRESSION_LEVEl = 3;

//     size_t m_size = 0;
//     mutable sg::shared_c_buffer<T> m_raw_buffer;
//     mutable sg::shared_c_buffer<uint8_t> m_compressed_data;
//     mutable std::weak_ptr<IView<T>> m_weakptr;

//     int m_cLevel = DEFAULT_COMPRESSION_LEVEl;
//     int m_noThread = DEFAULT_COMP_THREAD_COUNT;

//     void uncompress_data() const {
//         if (!m_compressed_data.get() || m_compressed_data.size() == 0)
//             return;

//         m_raw_buffer = sg::compression::zstd::decompress<T>(m_compressed_data);
//         m_compressed_data.reset();
//     }
//     void compress_data() const {
//         if (!m_raw_buffer.get() || m_raw_buffer.size() == 0)
//             return;

//         m_compressed_data = sg::compression::zstd::compress(m_raw_buffer, m_cLevel, m_noThread);
//         m_raw_buffer.reset();
//     }

//   public:
//     compressed_channel() = default;
//     compressed_channel(std::string name, std::vector<std::string> hierarchy) {
//         this->name(name);
//         this->hierarchy(hierarchy);
//     }
//     compressed_channel(std::string name,
//                        std::vector<std::string> hierarchy,
//                        sg::unique_c_buffer<T> &&inBuffer) {
//         this->name(name);
//         this->hierarchy(hierarchy);
//         set_data(inBuffer);
//     }

//     void compression_level(int cLevel) { m_cLevel = cLevel; };
//     void compresstion_thread_count(int noThread) { m_noThread = noThread; };

//     void set_data(sg::unique_c_buffer<T> &&inBuffer) {
//         m_size = inBuffer.size();
//         T* ptr = inBuffer.release();
//         m_raw_buffer = sg::shared_c_buffer<T>(ptr, m_size);
//         compress_data();
//     }

//     std::shared_ptr<IView<T>> data_view() const {
//         /* If ther is already a shared_ptr, return that */
//         auto ptr = m_weakptr.lock();
//         if (ptr)
//             return ptr;

//         uncompress_data();

//         ptr = std::shared_ptr<view_compressed<T>>(new view_compressed<T>(*this, m_raw_buffer.get()), [this](view_compressed<T> *p) {
//             this->compress_data();
//             delete p;
//         });
//         m_weakptr = ptr;

//         return m_weakptr.lock();
//     }

//     // IChannel interface
//   public:
//     size_t size() const noexcept { return m_size; };
// };

// } // namespace sg::data
