#include "sg/buffer.h"
#include "sg/file_writer.h"
#include "sg/libuv_wrapper.h"

#include <cstring>
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
#include <future>

namespace sg {

class file_writer::impl : public sg::enable_lifetime_indicator {
  public:
    impl(sg::file_writer& parent, sg::libuv_wrapper& libuv)
        : m_parent(parent),
          m_libuv(libuv){}

    void start(std::filesystem::path _path,
               file_writer::error_cb_t on_error_cb,
               file_writer::started_cb_t on_client_connected_cb,
               file_writer::stopped_cb_t on_client_disconnected_cb,
               unsigned int write_interval = 1000) {

        if (is_running())
            throw std::logic_error("this file_write is currently running");

        m_path = _path;
        buffer_write_interval = write_interval;

        /* set parent cbs */
        m_on_error_cb = on_error_cb;
        m_on_client_connected_cb = on_client_connected_cb;
        m_on_client_disconnected_cb = on_client_disconnected_cb;

        m_connection_promise = std::promise<void>();
        m_connection_future = m_connection_promise.get_future();

        std::function<void(sg::libuv_wrapper*)> setup_func = [this](sg::libuv_wrapper* wrap){
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
        std::function<sg::libuv_wrapper::wrapup_result(sg::libuv_wrapper*)> wrapup_func = [&](sg::libuv_wrapper*){
            bool can_stop;
            do_write(can_stop);

            if (can_stop)
                return sg::libuv_wrapper::wrapup_result::stop_uv_loop;
            return sg::libuv_wrapper::wrapup_result::rerun_uv_loop;
        };

        sg::libuv_wrapper::cb_t setup_cb = sg::create_weak_function(this, setup_func);
        sg::libuv_wrapper::cb_wrapup_t wrapup_cb = sg::create_weak_function(this, wrapup_func);

        m_libuv_task_index = m_libuv.start_task(setup_cb, wrapup_cb);
        m_connection_future.get();
    }

    void write(sg::shared_c_buffer<std::byte> buf) {
        if (this->is_stopped_or_stopping())
            throw std::runtime_error("this file_writer is closed or closing");

        std::lock_guard lock(m_mutex);              
        m_buffer_in.emplace_back(std::move(buf));
    }

    std::filesystem::path path() const { return open_req.path; }

    void close_async()
    {
        m_libuv.stop_async();
    }

    void close() {
        m_libuv.stop();
    }

    bool is_running() const {
        return !m_libuv.is_stopped();
    }

    bool is_stopped_or_stopping() {
        return m_libuv.is_stopped_or_stopping();
    }

    ~impl() {
        close();
    }

  private:
    sg::file_writer& m_parent;
    std::filesystem::path m_path;

    sg::libuv_wrapper& m_libuv;
    size_t m_libuv_task_index;

    /* client callbacks */
    error_cb_t m_on_error_cb;
    file_writer::started_cb_t m_on_client_connected_cb;
    file_writer::stopped_cb_t m_on_client_disconnected_cb;

    std::shared_mutex m_mutex;
    std::vector<sg::shared_c_buffer<std::byte>> m_buffer_in;
    std::vector<sg::shared_c_buffer<std::byte>> m_buffer_out;

    std::promise<void> m_connection_promise;
    std::future<void> m_connection_future;

    bool m_write_pending = false;
    unsigned int buffer_write_interval;

    /* libuv objects */
    uv_fs_t open_req;
    uv_fs_t write_req;
    std::vector<uv_buf_t> m_uv_buffers;
    uv_timer_t m_uv_timer;

    void do_write(bool& can_terminate) {
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
                m_uv_buffers.emplace_back(
                    uv_buf_init(reinterpret_cast<char *>(buf.get()), buf.size()));

            write_req.data = this;

            m_write_pending = true;
            uv_fs_write(m_libuv.get_uv_loop(),
                        &write_req,
                        static_cast<uv_file>(open_req.result),
                        m_uv_buffers.data(),
                        m_uv_buffers.size(),
                        -1,
                        on_uv_on_write);
            return;
        }

        can_terminate=true;
    }

    /* libuv callack functions */
    void on_error(const std::string &message) {
        if (m_on_error_cb)
            m_on_error_cb(message);
    }
    static void on_uv_open(uv_fs_t *req) {
        auto res = req->result;
        auto a = (file_writer::impl *)req->data;

        uv_fs_req_cleanup(req);

        try {
            if (res < 0)
                throw std::runtime_error(uv_strerror(static_cast<int>(res)));

            /* The timer is only started after the file is opened, by on_uv_open*/
            THROW_ON_LIBUV_ERROR(uv_timer_init(req->loop, &a->m_uv_timer));
            a->m_uv_timer.data = a;

            THROW_ON_LIBUV_ERROR(uv_timer_start(&a->m_uv_timer,
                                                &on_uv_timer_tick,
                                                a->buffer_write_interval,
                                                a->buffer_write_interval));
        } catch (...) {
            a->m_connection_promise.set_exception(std::current_exception());
            return;
        }

        if (a->m_on_client_connected_cb)
            a->m_on_client_connected_cb(&a->m_parent);

        a->m_connection_promise.set_value();
    }

    static void on_uv_on_write(uv_fs_t *req) {
        auto a = (file_writer::impl *)req->data;

        auto res = req->result;
        uv_fs_req_cleanup(req);

        {
            std::lock_guard lock(a->m_mutex);
            a->m_write_pending = false;
        }

        if (res < 0)
            a->on_error(uv_strerror(static_cast<int>(res)));
    }

    static void on_uv_timer_tick(uv_timer_t *handle) {
        /* Only called on UV event loop, so we don't need to lock for some things*/
        auto a = (file_writer::impl *)handle->data;
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
};

file_writer::file_writer():
    pimpl(*this, sg::get_global_uv_holder()) {};

file_writer::~file_writer() {
    //pimpl->stop();
}

void file_writer::start(const std::filesystem::path &_path,
                        file_writer::error_cb_t on_error_cb,
                        file_writer::started_cb_t on_client_connected_cb,
                        file_writer::stopped_cb_t on_client_disconnected_cb,
                        unsigned int write_interval) {
    pimpl->start(_path, on_error_cb, on_client_connected_cb, on_client_disconnected_cb, write_interval);
}

void file_writer::stop() { pimpl->close(); }

bool file_writer::is_running() const { return pimpl->is_running(); }

void file_writer::write(sg::shared_c_buffer<std::byte> buf)
{
    pimpl->write(std::move(buf));
}

void file_writer::write(const char *data, size_t length) {
    auto a =sg::make_shared_c_buffer<std::byte>(length);
    std::memcpy(a.get(), data, length*sizeof(char));
    pimpl->write(std::move(a));
}

void file_writer::write(const std::string_view &msg) {
    /* having this in header means that we don't have to worry about
     * passing std::string across library boundaries. */
    write(msg.data(), msg.size());
}

void file_writer::write_line(const std::string_view &msg) {
    /* having this in header means that we don't have to worry about
     * passing std::string across library boundaries. */
    #ifdef _WIN32
    write(std::string(msg) + "\r\n");
    #else
    write(std::string(msg) + "\n");
    #endif
}

std::filesystem::path file_writer::path() const { return pimpl->path(); }

} // namespace sg
