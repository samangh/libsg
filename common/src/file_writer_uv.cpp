#include "sg/file_writer_uv.h"
#include "sg/buffer.h"
#include "sg/libuv_wrapper.h"

#include <cstring>
#include <filesystem>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace sg {

class file_writer_uv::impl : public sg::enable_lifetime_indicator {
  public:
    impl(sg::file_writer_uv &parent);
    ~impl();

    void start(std::filesystem::path _path,
               file_writer_uv::error_cb_t on_error_cb,
               file_writer_uv::stopped_cb_t on_client_disconnected_cb,
               unsigned int write_interval = 1000);

    void write_async(sg::shared_c_buffer<std::byte> buf);
    void write(sg::shared_c_buffer<std::byte> buf);

    void close_async();
    void close();
    bool is_running() const;
    bool is_stopped_or_stopping();

    std::filesystem::path path() const;

  private:
    sg::file_writer_uv &m_parent;
    std::filesystem::path m_path;

    sg::libuv_wrapper m_libuv;
    size_t m_libuv_task_index;

    /* client callbacks */
    error_cb_t m_on_error_cb;
    file_writer_uv::stopped_cb_t m_on_client_disconnected_cb;

    std::recursive_mutex m_mutex;
    std::vector<sg::shared_c_buffer<std::byte>> m_buffer_in;
    std::vector<sg::shared_c_buffer<std::byte>> m_buffer_out;

    std::promise<void> m_connection_promise;

    bool m_write_pending = false;
    unsigned int buffer_write_interval;

    void do_write(bool &can_terminate);

    /* libuv objects */
    uv_fs_t open_req;
    uv_fs_t close_req;
    uv_fs_t write_req;
    std::vector<uv_buf_t> m_uv_buffers;
    uv_timer_t m_uv_timer;

    /* libuv callack functions */
    void on_error(const std::string &message);
    static void on_uv_open(uv_fs_t *req);
    static void on_uv_on_write(uv_fs_t *req);
    static void on_uv_timer_tick(uv_timer_t *handle);
};

/*******************************************************************
 * Public implementation
 ******************************************************************/

file_writer_uv::file_writer_uv() : pimpl(*this){};

file_writer_uv::~file_writer_uv() = default;

void file_writer_uv::start(const std::filesystem::path &_path,
                        file_writer_uv::error_cb_t on_error_cb,
                        file_writer_uv::stopped_cb_t on_client_disconnected_cb,
                        unsigned int write_interval) {
    pimpl->start(
        _path, on_error_cb, on_client_disconnected_cb, write_interval);
}

void file_writer_uv::stop() { pimpl->close(); }

bool file_writer_uv::is_running() const { return pimpl->is_running(); }

void file_writer_uv::write_async(sg::shared_c_buffer<std::byte> buf) { pimpl->write_async(std::move(buf)); }
void file_writer_uv::write(sg::shared_c_buffer<std::byte> buf) { pimpl->write(std::move(buf)); }

void file_writer_uv::write_async(const char *data, size_t length) {
    auto a = sg::make_shared_c_buffer<std::byte>(length);
    std::memcpy(a.get(), data, length * sizeof(char));
    pimpl->write_async(std::move(a));
}

/*******************************************************************
 * Private implementation
 ******************************************************************/

void file_writer_uv::write_async(const std::string_view &msg) {
    /* having this in header means that we don't have to worry about
     * passing std::string across library boundaries. */
    write_async(msg.data(), msg.size());
}

void file_writer_uv::write_line_async(const std::string_view &msg) {
/* having this in header means that we don't have to worry about
 * passing std::string across library boundaries. */
#ifdef _WIN32
    write_async(std::string(msg) + "\r\n");
#else
    write_async(std::string(msg) + "\n");
#endif
}

std::filesystem::path file_writer_uv::path() const { return pimpl->path(); }

void file_writer_uv::impl::start(std::filesystem::path _path,
                              error_cb_t on_error_cb,
                              stopped_cb_t on_client_disconnected_cb,
                              unsigned int write_interval) {
    if (is_running())
        throw std::logic_error("this file_write is currently running");

    m_path = _path;
    buffer_write_interval = write_interval;

    /* set parent cbs */
    m_on_error_cb = on_error_cb;
    m_on_client_disconnected_cb = on_client_disconnected_cb;

    m_connection_promise = std::promise<void>();

    std::function<void(sg::libuv_wrapper *)> setup_func = [this](sg::libuv_wrapper *wrap) {
        {
            std::lock_guard lock(m_mutex);
            m_buffer_in.clear();
            m_buffer_out.clear();
        }

        open_req.data = this;
        THROW_ON_LIBUV_ERROR(uv_fs_open(wrap->get_uv_loop(),
                                        &open_req,
                                        m_path.string().c_str(),
                                        UV_FS_O_TRUNC | UV_FS_O_CREAT | UV_FS_O_WRONLY |
                                            UV_FS_O_SEQUENTIAL | UV_FS_O_EXLOCK,
                                        0644,
                                        on_uv_open));
    };
    std::function<sg::libuv_wrapper::wrapup_result(sg::libuv_wrapper *)> wrapup_func =
        [&](sg::libuv_wrapper * wrap) {
            bool can_stop;
            do_write(can_stop);

            if (can_stop)
            {
                // Don't care about errors on closing the handle
                uv_fs_close(wrap->get_uv_loop(), &this->close_req , (uv_file)this->open_req.result, NULL);
                return sg::libuv_wrapper::wrapup_result::stop_uv_loop;
            }
            return sg::libuv_wrapper::wrapup_result::rerun_uv_loop;
        };

    std::function<void(sg::libuv_wrapper *)> stopped_func = [&, this](sg::libuv_wrapper *) {
        if (m_on_client_disconnected_cb)
            m_on_client_disconnected_cb(&m_parent);
    };

    m_libuv_task_index = m_libuv.start_task(setup_func, wrapup_func);
    m_libuv.add_on_stopped_cb(stopped_func);

    m_connection_promise.get_future().get();
}

void file_writer_uv::impl::write_async(sg::shared_c_buffer<std::byte> buf) {
    if (this->is_stopped_or_stopping())
        throw std::runtime_error("this file_writer is closed or closing");

    std::lock_guard lock(m_mutex);
    m_buffer_in.emplace_back(std::move(buf));
}

void file_writer_uv::impl::write(sg::shared_c_buffer<std::byte> buf) {
    if (this->is_stopped_or_stopping())
        throw std::runtime_error("this file_writer is closed or closing");

    std::lock_guard lock(m_mutex);
    auto total_written = 0;

    while ((size_t)total_written < buf.size()) {
        uv_fs_t _req;
        auto _buffer = uv_buf_init(reinterpret_cast<char *>(&buf.get()[total_written]),
                                   static_cast<unsigned int>(buf.size()) - total_written);
        auto result = uv_fs_write(m_libuv.get_uv_loop(),
                                  &_req,
                                  static_cast<uv_file>(open_req.result),
                                  &_buffer,
                                  1,
                                  -1,
                                  nullptr);
        THROW_ON_LIBUV_ERROR(result);
        total_written += result;
    }
}

std::filesystem::path file_writer_uv::impl::path() const { return open_req.path; }

void file_writer_uv::impl::close_async() { m_libuv.stop_async(); }

void file_writer_uv::impl::close() { m_libuv.stop(); }

bool file_writer_uv::impl::is_running() const { return !m_libuv.is_stopped(); }

bool file_writer_uv::impl::is_stopped_or_stopping() { return m_libuv.is_stopped_or_stopping(); }

file_writer_uv::impl::impl(file_writer_uv &parent)
    : m_parent(parent) {}

file_writer_uv::impl::~impl() { close(); }

void file_writer_uv::impl::do_write(bool &can_terminate) {
    can_terminate = false;

    /* This can be called by in event loop by on_uv_timer_tick(..).
     * However, m_buffer_in will be written to by other threads, so a lock is needed */
    std::lock_guard lock(m_mutex);

    /* If a write is already requested, then RETURN*/
    if (m_write_pending)
        return;

    if (m_buffer_in.size() > 0) {
        m_buffer_out.clear();
        m_buffer_in.swap(m_buffer_out);

        m_uv_buffers.clear();
        for (auto &buf : m_buffer_out)
            m_uv_buffers.emplace_back(uv_buf_init(reinterpret_cast<char *>(buf.get()), static_cast<unsigned int>(buf.size())));

        write_req.data = this;

        m_write_pending = true;
        uv_fs_write(m_libuv.get_uv_loop(),
                    &write_req,
                    static_cast<uv_file>(open_req.result),
                    m_uv_buffers.data(),
                    static_cast<unsigned int>(m_uv_buffers.size()),
                    -1,
                    on_uv_on_write);
        return;
    }

    can_terminate = true;
}

void file_writer_uv::impl::on_error(const std::string &message) {
    if (m_on_error_cb)
        m_on_error_cb(message);
}

void file_writer_uv::impl::on_uv_open(uv_fs_t *req) {
    auto res = req->result;
    auto a = (file_writer_uv::impl *)req->data;

    uv_fs_req_cleanup(req);

    try {
        if (res < 0)
            throw std::runtime_error(uv_strerror(static_cast<int>(res)));

        /* The timer is only started after the file is opened, by on_uv_open*/
        THROW_ON_LIBUV_ERROR(uv_timer_init(req->loop, &a->m_uv_timer));
        a->m_uv_timer.data = a;

        THROW_ON_LIBUV_ERROR(uv_timer_start(
            &a->m_uv_timer, &on_uv_timer_tick, a->buffer_write_interval, a->buffer_write_interval));
    } catch (...) {
        a->m_connection_promise.set_exception(std::current_exception());
        return;
    }

    a->m_connection_promise.set_value();
}

void file_writer_uv::impl::on_uv_on_write(uv_fs_t *req) {
    auto a = (file_writer_uv::impl *)req->data;

    auto res = req->result;
    uv_fs_req_cleanup(req);

    /* if errors */
    if (res < 0)
    {
        {
            std::lock_guard lock(a->m_mutex);
            a->m_write_pending = false;
        }

        a->on_error(uv_strerror(static_cast<int>(res)));
        return;
    }

    std::lock_guard lock(a->m_mutex);
    a->m_write_pending = false;


    /* rest is to check that bufferis fully written, as event if there is
     * no error its possible that not all data has been written */

    /* size of buffers that were supposed to be written */
    size_t total_size = 0;
    size_t written_size = static_cast<size_t>(res);
    for (const auto &buf : a->m_buffer_out)
        total_size += buf.size();

    /* if write is not complete */
    if (written_size < total_size) {
        /* move and clear out existing out buffer */
        auto buffers = std::move(a->m_buffer_out);
        auto remaning_count = written_size;

        for (auto it = buffers.cbegin(); it != buffers.cend();) {
            /* this buffer has been fully written, skip */
            if (remaning_count >= it->size()) {
                written_size -= it->size();
                it = buffers.erase(it);
                continue;
            }

            /* if a (partially) unwritten buffer*/
            if (written_size < it->size()) {
                auto remainder_count = it->size() - written_size;
                auto new_buf = sg::make_shared_c_buffer<std::byte>(remainder_count);
                std::memcpy(new_buf.get(), &it->get()[written_size - 1], remainder_count);

                /* Add buffers to any existing input buffer */
                a->m_buffer_in.insert(a->m_buffer_in.cbegin(), new_buf);
                a->m_buffer_in.insert(a->m_buffer_in.cbegin(), std::next(it), buffers.cend());
                break;
            }
        }

        bool _temp;
        a->do_write(_temp);
    }
}

void file_writer_uv::impl::on_uv_timer_tick(uv_timer_t *handle) {
    /* Only called on UV event loop, so we don't need to lock for some things*/
    auto a = (file_writer_uv::impl *)handle->data;
    bool can_terminate;

    /* Note (from
     * https://stackoverflow.com/questions/47401833/is-uv-write-actually-asynchronous):
     *
     * Filesysme IO operations run on thread pool. This is because there is no good and portable
     * way to do non-blocking IO for files. Because we use a thread pool, write operations can
     * run in parallel. That's why uv_fs_write takes an offset parameter, so multiple threas can
     * write without stepping on top of each other.
     *
     * Because I don't want to maintain the offset paramemter I have one single buffer that I
     * write at a time. */
    a->do_write(can_terminate);
}

} // namespace sg
