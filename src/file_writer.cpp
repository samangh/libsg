#include <sg/file_writer.h>


#include <uv.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <vector>
#include <functional>
#include <stdexcept>

#define THROW_ON_ERROR(err) if (err<0) throw std::runtime_error(uv_strerror(err));

namespace sg {

class file_writer::impl {
  public:
    impl();
    ~impl();
    void start(std::filesystem::path _path,
               error_cb_t on_error_cb,
               started_cb_t on_client_connected_cb,
               stopped_cb_t on_client_disconnected_cb,
               unsigned int write_interval = 1000);
    void stop();
    bool is_running() const;

    void write(const char *data, size_t length);

    std::filesystem::path path() const;

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

void file_writer::impl::on_uv_timer_tick(uv_idle_t *handle) {
    /* Only called on UV event loop, so we don't need to lock for some things*/
    auto a = (file_writer::impl *)handle->loop->data;

    /* If a write is already requested, then RETURN*/
    if (a->m_write_pending)
        return;

    /* If data is available, send write request and RETURN*/
    if (uv_now(handle->loop) >= a->m_last_write_time + a->buffer_write_interval) {
        a->m_last_write_time = uv_now(handle->loop);

        std::lock_guard lock(a->m_mutex);
        if (a->m_buffer_in->size() > 0) {
            a->m_buffer_out = std::move(a->m_buffer_in);
            a->m_buffer_in = std::make_unique<std::vector<char>>();

            auto size = a->m_buffer_out->size();
            auto buff_ptr = &(*a->m_buffer_out.get())[0];
            a->m_uv_buf = uv_buf_init(buff_ptr, static_cast<unsigned int>(size));

            a->m_write_pending = true;
            uv_fs_write(&a->m_loop, &a->write_req, static_cast<uv_file>(a->open_req.result), &a->m_uv_buf, 1, -1, on_uv_on_write);

            return;
        }
    }

    /* Stop requested.
    * We only check for this after there is not further data to write. */
    std::shared_lock lock(a->m_stop_mutex);
    if (a->m_stop_requested) {
        uv_fs_close(&a->m_loop, &a->close_req, static_cast<uv_file>(a->open_req.result), &file_writer::impl::on_uv_on_file_close);
        uv_idle_stop(handle);
        uv_stop(&a->m_loop);
    }
}

void file_writer::impl::write(const char *data, size_t length) {
    std::lock_guard lock(m_mutex);
    m_buffer_in.get()->insert(m_buffer_in.get()->end(), data, data + length);
}


file_writer::impl::impl():m_buffer_in(std::make_unique<std::vector<char>>()){}

file_writer::impl::~impl()
{
    if (is_running())
        stop();
}

void file_writer::impl::start(std::filesystem::path path, error_cb_t on_error_cb,
                              started_cb_t on_client_connected_cb,
                        stopped_cb_t on_client_disconnected_cb,
                        unsigned int write_interval) {
    if (is_running())
        throw std::logic_error("this file writer is currently running");

    m_error_cb = on_error_cb;
    m_started_cb = on_client_connected_cb;
    m_stopped_cb = on_client_disconnected_cb;
    buffer_write_interval = write_interval;

    /* setup UV loop */
    THROW_ON_ERROR(uv_loop_init(&m_loop));
    m_loop.data = this;

    THROW_ON_ERROR(uv_fs_open(&m_loop, &open_req, path.string().c_str(), UV_FS_O_TRUNC | UV_FS_O_CREAT | UV_FS_O_WRONLY | UV_FS_O_SEQUENTIAL | UV_FS_O_EXLOCK, 0644, on_uv_open));

    /* Set the last time the file buffer was checked to 0.
     * Note: the idler is started by the on_uv_open cb 
     */
    m_last_write_time = 0;

    m_thread = std::thread([&]() {
        if (m_started_cb)
            m_started_cb();
        while (true) {
            /*  Runs the event loop until there are no more active and referenced
             *  handles or requests.  Returns non-zero if uv_stop() was called
             *  and there are still active handles or  requests. Returns zero in
             *  all other cases. */
            uv_run(&m_loop, UV_RUN_DEFAULT);

            /* The following loop closing logic is from guidance from
             * https://stackoverflow.com/questions/25615340/closing-libuv-handles-correctly
             *
             *  If there are any loops that are not closing:
             *
             *  - Use uv_walk and call uv_close on the handles;
             *  - Run the loop again with uv_run so all close callbacks are
             *    called and you can free the memory in the callbacks */

            /* Close callbacks */
            uv_walk(
                &m_loop,
                [](uv_handle_s *handle, void *) { uv_close(handle, nullptr); },
                nullptr);

            /* Check if there are any remaining callbacks*/
            if (uv_loop_close(&m_loop) != UV_EBUSY)
                break;
        }
        if (m_stopped_cb)
            m_stopped_cb();
    });
}

std::filesystem::path file_writer::impl::path() const {
    return open_req.path;
}

void file_writer::impl::stop() {
    {
        std::lock_guard lock(m_mutex);
        m_stop_requested = true;
    }

    // join and wait for the thread to end.
    // Note: A thread that has finished executing code, but has not yet been joined
    //        is still considered an active thread of execution and is therefore joinable.
    if (m_thread.joinable())
        m_thread.join();
}

bool file_writer::impl::is_running() const {
    return m_thread.joinable() && (uv_loop_alive(&m_loop) != 0);
}

void file_writer::impl::on_error(const std::string &message) {
    if (m_error_cb)
        m_error_cb(message);
}

void file_writer::impl::on_uv_open(uv_fs_t *req) {
    auto res = req->result;
    uv_fs_req_cleanup(req);
    auto a = (file_writer::impl *)req->loop->data;

    if (res < 0) {
        a->on_error(uv_strerror(static_cast<int>(res)));
        return;
    }

    /* The idler is only started after the file is opened, by on_uv_open*/
    uv_idle_init(&a->m_loop, &a->m_idler);
    uv_idle_start(&a->m_idler, &on_uv_timer_tick);
}

void file_writer::impl::on_uv_on_write(uv_fs_t *req) {
    auto res = req->result;
    uv_fs_req_cleanup(req);

    auto a = (file_writer::impl *)req->loop->data;
    a->m_write_pending = false;
    if (res < 0)
        a->on_error(uv_strerror(static_cast<int>(res)));
}

void file_writer::impl::on_uv_on_file_close(uv_fs_t *req) {
    auto res = req->result;
    uv_fs_req_cleanup(req);

    auto a = (file_writer::impl *)req->loop->data;
    if (res < 0)
        a->on_error(uv_strerror(static_cast<int>(res)));
}

file_writer::file_writer() = default;
file_writer::~file_writer() = default;

void file_writer::start(std::filesystem::__cxx11::path _path, file_writer::error_cb_t on_error_cb, file_writer::started_cb_t on_client_connected_cb, file_writer::stopped_cb_t on_client_disconnected_cb, unsigned int write_interval)
{
    pimpl->start(_path, on_error_cb, on_client_connected_cb, on_client_disconnected_cb,write_interval);
}

void file_writer::stop()
{
    pimpl->stop();
}

bool file_writer::is_running() const
{
    return pimpl->is_running();
}

void file_writer::write(const char *data, size_t length)
{
    pimpl->write(data, length);
}


std::filesystem::__cxx11::path file_writer::path() const
{
    return pimpl->path();
}

} // namespace sg
