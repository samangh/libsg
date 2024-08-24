#include <sg/libuv_wrapper.h>

namespace sg {

libuv_wrapper::~libuv_wrapper() { stop(); }

void libuv_wrapper::stop() {
    stop_async();
    block_until_stopped();
}

bool libuv_wrapper::is_stopped() const noexcept { return !m_uvloop_running; }

bool libuv_wrapper::is_stopped_or_stopping() const noexcept {
    return m_uvloop_stop_requested || is_stopped();
}

void libuv_wrapper::block_until_stopped() const {
    /* need mutex as join() is not threadsafe */
    std::lock_guard lock(m_start_stop_mutex);

    /* A thread that has finished executing code, but has not yet been
     * joined is still considered an active thread of execution and is
     * therefore joinable. */
    if (m_thread.joinable())
        m_thread.join();
}

uv_loop_t *libuv_wrapper::get_uv_loop() { return &m_loop; }

void libuv_wrapper::add_on_loop_started_cb(cb_t cb) {
    std::lock_guard lock(m_tasks_mutex);
    m_started_cbs.push_back(cb);
}

void libuv_wrapper::add_on_stopped_cb(cb_t cb) {
    std::lock_guard lock(m_tasks_mutex);
    m_stopped_cbs.push_back(cb);
}

void libuv_wrapper::start_libuv() {
    std::lock_guard lock(m_start_stop_mutex);
    std::lock_guard lock_tasks(m_tasks_mutex);

    if (!is_stopped_or_stopping())
        throw std::logic_error("this libuv loop is currently running");

    /* If libuv loop is stopped (or process of stopping), but the thread not joined yet do it */
    block_until_stopped();
    m_uvloop_stop_requested = false;

    /* setup UV loop */
    THROW_ON_LIBUV_ERROR(uv_loop_init(&m_loop));
    m_loop.data = this;

    /* task setup callacks */
    for (auto& [index, cb] : libuv_setup_task_cbs)
        cb(this);
    libuv_setup_task_cbs.clear();

    /* Setup handle for stopping the loop */
    m_async = std::make_unique<uv_async_t>();
    uv_async_init(&m_loop, m_async.get(), [](uv_async_t *handle) {
        uv_stop(handle->loop);
    });

    m_thread = std::thread([&, this]() {
        {
            std::lock_guard lock(m_tasks_mutex);
            for (auto &cb : m_started_cbs)
                cb(this);
            m_started_cbs.clear();
        }

        m_uvloop_running = true;
        bool loop_stop_requested = false;

        while (true) {
            /*  Runs the event loop until there are no more active and referenced
             *  handles or requests.  Returns non-zero if uv_stop() was called
             *  and there are still active handles or  requests. Returns zero in
             *  all other cases. */
            if (loop_stop_requested)
                uv_run(&m_loop, UV_RUN_ONCE);
            else
                uv_run(&m_loop, UV_RUN_DEFAULT);

            loop_stop_requested = true;
            {
                std::lock_guard lock_tasks(m_tasks_mutex);
                for (auto it = m_wrapup_tasks_cbs.begin(); it != m_wrapup_tasks_cbs.end();)
                    it = (it->second(this) == wrapup_result::stop_uv_loop)
                             ? m_wrapup_tasks_cbs.erase(it)
                             : std::next(it);

                if (m_wrapup_tasks_cbs.size() > 0)
                    continue;
            }

            /* The following loop closing logic is from guidance from
             * https://stackoverflow.com/questions/25615340/closing-libuv-handles-correctly
             *
             *  If there are any loops that are not closing:
             *
             *  - Use uv_walk and call uv_close on the handles;
             *  - Run the loop again with uv_run so all close callbacks are
             *    called and you can free the memory in the callbacks */

            /* Close callbacks */
            uv_walk(&m_loop, [](uv_handle_s *handle, void *) {
                uv_close(handle, nullptr);
            }, nullptr);

            /* Check if there are any remaining callbacks*/
            if (uv_loop_close(&m_loop) != UV_EBUSY)
                break;
        }

        m_uvloop_running = false;

        {
            std::lock_guard lock(m_tasks_mutex);
            for (auto &cb : m_stopped_cbs)
                cb(this);
            m_stopped_cbs.clear();
        }
    });
}

size_t libuv_wrapper::add_setup_task_cb(cb_t setup, cb_wrapup_t wrapup)
{
    std::lock_guard lock(m_tasks_mutex);
    auto index = m_task_counter++;

    libuv_setup_task_cbs.emplace(index, std::move(setup));
    m_wrapup_tasks_cbs.emplace(index, std::move(wrapup));

    return index;
}

void libuv_wrapper::remove_task_callbacks(size_t index)
{
    std::lock_guard lock(m_tasks_mutex);
    m_wrapup_tasks_cbs.erase(index);
    libuv_setup_task_cbs.erase(index);
}

size_t libuv_wrapper::start_task(cb_t setup, cb_wrapup_t wrapup) {
    size_t i;

    if (is_stopped_or_stopping()) {
        block_until_stopped();

        i = add_setup_task_cb(std::move(setup), std::move(wrapup));
        start_libuv();
    } else {
        i = add_setup_task_cb(nullptr, std::move(wrapup));
        setup(this);
    }

    return i;
}

void libuv_wrapper::stop_async() {
    if (!m_uvloop_running)
        return;

    auto stop_requested_previously = m_uvloop_stop_requested.exchange(true);
    if (stop_requested_previously)
        return;

    /* uv_async_send can throw error if we are in the process walking
     * the uv_loop callbacks, so ensure we do it only once */
    THROW_ON_LIBUV_ERROR(uv_async_send(m_async.get()));
}

libuv_wrapper& get_global_uv_holder() {
    static libuv_wrapper ptr;
    return ptr;
}

} // namespace sg
