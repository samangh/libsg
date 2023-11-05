#include <sg/file_writer.h>
#include <sg/libuv_wrapper.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace sg {

class file_writer::impl : public sg::libuv_wrapper {
  public:
    impl();
    void start(std::filesystem::path _path,
               error_cb_t on_error_cb,
               libuv_on_start_cb_t on_client_connected_cb,
               libuv_on_stop_cb_t on_client_disconnected_cb,
               unsigned int write_interval = 1000);

    void write(const char *data, size_t length);

    std::filesystem::path path() const;

  private:
    std::filesystem::path m_path;

    void on_error(const std::string &message);
    static void on_uv_open(uv_fs_t *req);
    static void on_uv_on_write(uv_fs_t *req);
    static void on_uv_on_file_close(uv_fs_t *req);
    static void on_uv_timer_tick(uv_idle_t *handle);

    std::unique_ptr<std::vector<char>> m_buffer_in;
    std::unique_ptr<std::vector<char>> m_buffer_out;

    bool m_write_pending = false;
    uint64_t m_last_write_time;
    unsigned int buffer_write_interval;

    uv_idle_t m_idler;

    std::shared_mutex m_mutex;

    error_cb_t m_error_cb;

    uv_fs_t open_req;
    uv_fs_t write_req;
    uv_fs_t close_req;
    uv_buf_t m_uv_buf;

    void setup_libuv_operations() override;
    void stop_libuv_operations() override;
};

void file_writer::impl::on_uv_timer_tick(uv_idle_t *handle) {
    /* Only called on UV event loop, so we don't need to lock for some things*/
    auto a = (file_writer::impl *)handle->loop->data;

    /* If a write is already requested, then RETURN*/
    if (a->m_write_pending)
        return;

    /* Note (from https://stackoverflow.com/questions/47401833/is-uv-write-actually-asynchronous):
     *
     * Filesysme IO operations run on thread pool. This is because there is no good and portable way
     * to do non-blocking IO for files. Because we use a thread pool, write operations can run in
     * parallel. That's why uv_fs_write takes an offset parameter, so multiple threas can write
     * without stepping on top of each other.
     *
     * Because I don't want to maintain the offset paramemter I have one single buffer that I write
     * at a time. */

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
            uv_fs_write(&a->m_loop,
                        &a->write_req,
                        static_cast<uv_file>(a->open_req.result),
                        &a->m_uv_buf,
                        1,
                        -1,
                        on_uv_on_write);

            return;
        }
    }
}

void file_writer::impl::setup_libuv_operations() {
    {
        std::lock_guard lock(m_mutex);
        m_buffer_in->clear();
        m_buffer_out->clear();

        /* Set the last time the file buffer was checked to 0.
         * Note: the idler is started by the on_uv_open cb
         */
        m_last_write_time = 0;
    }

    THROW_ON_LIBUV_ERROR(uv_fs_open(&m_loop,
                                    &open_req,
                                    m_path.string().c_str(),
                                    UV_FS_O_TRUNC | UV_FS_O_CREAT | UV_FS_O_WRONLY |
                                        UV_FS_O_SEQUENTIAL | UV_FS_O_EXLOCK,
                                    0644,
                                    on_uv_open));
}

void file_writer::impl::stop_libuv_operations() {
    uv_fs_close(&m_loop,
                &close_req,
                static_cast<uv_file>(open_req.result),
                &file_writer::impl::on_uv_on_file_close);
}

void file_writer::impl::write(const char *data, size_t length) {
    std::lock_guard lock(m_mutex);
    m_buffer_in.get()->insert(m_buffer_in.get()->end(), data, data + length);
}

file_writer::impl::impl() : m_buffer_in(std::make_unique<std::vector<char>>()) {}

void file_writer::impl::start(std::filesystem::path path,
                              error_cb_t on_error_cb,
                              libuv_on_start_cb_t on_client_connected_cb,
                              libuv_on_stop_cb_t on_client_disconnected_cb,
                              unsigned int write_interval) {

    m_path = path;
    m_error_cb = on_error_cb;
    buffer_write_interval = write_interval;

    start_libuv(on_client_connected_cb, on_client_disconnected_cb);
}

std::filesystem::path file_writer::impl::path() const { return open_req.path; }

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

void file_writer::start(std::filesystem::path _path,
                        file_writer::error_cb_t on_error_cb,
                        file_writer::started_cb_t on_client_connected_cb,
                        file_writer::stopped_cb_t on_client_disconnected_cb,
                        unsigned int write_interval) {

    auto started_cb = [&, on_client_connected_cb](libuv_wrapper *) {
        if (on_client_connected_cb)
            on_client_connected_cb(this);
    };

    auto stoped_cb = [&, on_client_disconnected_cb](libuv_wrapper *) {
        if (on_client_disconnected_cb)
            on_client_disconnected_cb(this);
    };

    pimpl->start(_path, on_error_cb, started_cb, stoped_cb, write_interval);
}

void file_writer::stop() { pimpl->stop(); }

bool file_writer::is_running() const { return pimpl->is_running(); }

void file_writer::write(const char *data, size_t length) { pimpl->write(data, length); }

std::filesystem::path file_writer::path() const { return pimpl->path(); }

} // namespace sg
