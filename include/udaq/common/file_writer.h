#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <uv.h>
#include <vector>

namespace udaq::common {

class file_writer {
  public:
    typedef std::function<void(const std::string &msg)> error_cb_t;
    typedef std::function<void(void)> started_cb_t;
    typedef std::function<void(void)> stopped_cb_t;

    file_writer();
    ~file_writer();
    void start(std::filesystem::path _path,
               error_cb_t on_error_cb,
               started_cb_t on_client_connected_cb,
               stopped_cb_t on_client_disconnected_cb,
               unsigned int write_interval = 1000);
    void stop();
    bool is_running();

    void write(const char *data, size_t length);
    void write(const std::string &msg);
    void write_line(const std::string &msg);

    std::filesystem::path path();

  private:
    void on_error(const std::string &message);

    static void on_uv_open(uv_fs_t *req);
    static void on_uv_on_write(uv_fs_t *req);
    static void on_uv_on_file_close(uv_fs_t *req);
    static void on_uv_timer_tick(uv_idle_t *handle);

    std::unique_ptr<std::vector<char>> m_buffer_in;
    std::unique_ptr<std::vector<char>> m_buffer_out;

    bool m_stop_requested = false;
    bool m_write_pending = false;

    uint64_t m_last_write_time;
    unsigned int buffer_write_interval;

    uv_loop_t m_loop;
    uv_idle_t m_idler;
    std::thread m_thread;

    std::shared_mutex m_mutex; /* Mutex for operations*/
    std::shared_mutex m_stop_mutex;

    error_cb_t m_error_cb;
    started_cb_t m_started_cb;
    stopped_cb_t m_stopped_cb;

    uv_fs_t open_req;
    uv_fs_t write_req;
    uv_fs_t close_req;
    uv_buf_t m_uv_buf;
};

} // namespace udaq::common
