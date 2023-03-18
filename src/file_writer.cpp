#include <functional>
#include <stdexcept>
#include <udaq/common/file_writer.h>
#include <uv.h>

#define THROW_ON_ERROR(err) if (err<0) throw std::runtime_error(uv_strerror(err));

namespace udaq::common {

file_writer::file_writer() : m_buffer_in(std::make_unique<std::vector<char>>()) {
}

void file_writer::on_uv_timer_tick(uv_idle_t *handle) {
    /* Only called on UV event loop, so we don't need to lock for some things*/
    auto a = (file_writer *)handle->loop->data;

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
        uv_fs_close(&a->m_loop, &a->close_req, static_cast<uv_file>(a->open_req.result), &file_writer::on_uv_on_file_close);
        uv_idle_stop(handle);
        uv_stop(&a->m_loop);
    }
}

void file_writer::write(const char *data, size_t length) {
    std::lock_guard lock(m_mutex);
    m_buffer_in.get()->insert(m_buffer_in.get()->end(), data, data + length);
}

void file_writer::write(const std::string &msg) {
    write(msg.c_str(), msg.size());
}

void file_writer::write_line(const std::string &msg) {
    write(msg + "\n");
}

void file_writer::start(std::filesystem::path path, file_writer::error_cb_t on_error_cb,
                        file_writer::started_cb_t on_client_connected_cb,
                        file_writer::stopped_cb_t on_client_disconnected_cb,
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
        m_stopped_cb();
    });
}

file_writer::~file_writer() {
    if (is_running())
        stop();
}

std::filesystem::path file_writer::path() {
    return open_req.path;
}

void file_writer::stop() {
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

bool file_writer::is_running() {
    return m_thread.joinable() && (uv_loop_alive(&m_loop) != 0);
}

void file_writer::on_error(const std::string &message) {
    if (m_error_cb != nullptr)
        m_error_cb(message);
}

void file_writer::on_uv_open(uv_fs_t *req) {
    auto res = req->result;
    uv_fs_req_cleanup(req);
    auto a = (file_writer *)req->loop->data;

    if (res < 0) {
        a->on_error(uv_strerror(static_cast<int>(res)));
        return;
    }

    /* The idler is only started after the file is opened, by on_uv_open*/
    uv_idle_init(&a->m_loop, &a->m_idler);
    uv_idle_start(&a->m_idler, &on_uv_timer_tick);
}

void file_writer::on_uv_on_write(uv_fs_t *req) {
    auto res = req->result;
    uv_fs_req_cleanup(req);

    auto a = (file_writer *)req->loop->data;
    a->m_write_pending = false;
    if (res < 0)
        a->on_error(uv_strerror(static_cast<int>(res)));
}

void file_writer::on_uv_on_file_close(uv_fs_t *req) {
    auto res = req->result;
    uv_fs_req_cleanup(req);

    auto a = (file_writer *)req->loop->data;
    if (res < 0)
        a->on_error(uv_strerror(static_cast<int>(res)));
}

} // namespace udaq::common
