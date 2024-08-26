#include "sg/file_writer.h"
#include "sg/debug.h"

#include <fmt/core.h>

#include <cstring>
#include <filesystem>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "sg/buffer.h"

namespace sg {

void sg::file_writer::action(const std::stop_token &stop_tok) {
    while(true) {
        /* wait for data, unless a stop has been requested */
        if (!stop_tok.stop_requested())
            m_signal.acquire();

        /* swap buffer */
        std::deque<sg::shared_c_buffer<std::byte>> m_old_data;
        {
            std::lock_guard lock(m_mutex);
            m_old_data.swap(m_data);
        }

        size_t count{0};
        try {
            while (!m_old_data.empty()) {
                auto buff = m_old_data.back();
                m_old_data.pop_back();
                m_file.write((char *)(buff.get()), buff.size());
                count += buff.size();
            }
            m_byte_count.fetch_add(count * sizeof(char));
        } catch (const std::exception &ex) {
            if (m_on_error_cb) m_on_error_cb(this, ex.what());
            break;
        }

        /* if stop is requested and there is data, go around one more loop */
        if (stop_tok.stop_requested())
        {
            std::lock_guard lock(m_mutex);
            if (m_data.empty())
                break;
        }
    };

    try {
        if (m_file.is_open()) m_file.close();
    } catch (...) {
    };

    if (m_on_stop_cb) m_on_stop_cb(this);
}

sg::file_writer::~file_writer() { stop(); }

void sg::file_writer::start(path_type _path,
                       error_cb_t on_error_cb,
                       started_cb_t on_start_cb,
                       stopped_cb_t on_stop_cb) {
    if (m_thread.joinable())
        throw std::runtime_error(fmt::format("this {} is already running", sg::type_name<file_writer>()));

    m_on_error_cb = std::move(on_error_cb);
    m_on_stop_cb = std::move(on_stop_cb);

    m_file = std::fstream(_path, std::ios::out | std::ios::binary | std::ios::trunc);
    m_thread = std::jthread([this](const std::stop_token &tok) { action(tok); });

    m_byte_count=0;

    if (on_start_cb) on_start_cb(this);
}

void sg::file_writer::stop() {
    if (m_thread.joinable()) {
        m_thread.request_stop();
        m_signal.release();
        m_thread.join();
    }
}

bool file_writer::is_running() const
{
    return m_thread.joinable();
}

void file_writer::write_aync(std::string_view view) {
    write_aync(view.data(), view.size());
}

size_t file_writer::bytes_transferred() const
{
    return m_byte_count.load();
}



}  // namespace sg
