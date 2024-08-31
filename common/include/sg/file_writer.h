#pragma once

#include "jthread.h"
#include "buffer.h"
#include "sg/export/common.h"

#include <cstring>
#include <deque>
#include <filesystem>
#include <functional>
#include <semaphore>
#include <string>
#include <fstream>
#include <mutex>


namespace sg {

/**
 * @brief       The file_writer class
 * @details     Note that the functions are generlly not thread safe, uless marked.
 */
class SG_COMMON_EXPORT file_writer {
   public:
    typedef std::function<void(file_writer *, const std::string &msg)> error_cb_t;
    typedef std::function<void(file_writer *)> started_cb_t;
    typedef std::function<void(file_writer *)> stopped_cb_t;

    typedef sg::shared_c_buffer<std::byte> buffer_type;
    typedef std::filesystem::path path_type;

    ~file_writer();

    /**
     * @brief stops the writer
     * @details note that this is not thread-safe
     */
    void start(path_type _path,
               error_cb_t on_error_cb,
               started_cb_t on_start_cb,
               stopped_cb_t on_stop_cb);

    /**
     * @brief stops the writer and flushes the queue
     * @details note that this is not thread-safe
     */
    void stop();
    [[nodiscard]] bool is_running() const;

    template<typename U>
    requires std::convertible_to<U, buffer_type>
    void write_aync(U&& buff) {
        std::lock_guard lock(m_mutex);
        m_data.push_back(std::forward<U>(buff));
        m_signal.release();
    }

    template<typename U>
    requires std::is_trivially_copyable_v<U>
    void write_aync(const U* ptr,  size_t length) {
        auto a = sg::make_shared_c_buffer<std::byte>(length*sizeof(U));
        std::memcpy(a.get(), ptr, length * sizeof(char));
        write_aync(std::move(a));
    }

    void write_aync(std::string_view view);

    [[nodiscard]] size_t bytes_transferred() const;
private:
    std::binary_semaphore m_signal{0};
    std::fstream m_file;

    std::jthread m_thread;

    error_cb_t m_on_error_cb;
    stopped_cb_t m_on_stop_cb;

    std::mutex m_mutex;
    std::deque<buffer_type> m_data;

    std::atomic<size_t> m_byte_count;

    void action(const std::stop_token &stop_tok);
};

} // namespace sg
