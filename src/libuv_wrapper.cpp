#include <sg/libuv_wrapper.h>

namespace sg {

libuv_wrapper::~libuv_wrapper() {
    stop();
}

void libuv_wrapper::stop() {
    /* Lock in case a separate thread is trying to close this at the same time */
    std::lock_guard lock(m_stopping_mutex);

    if(!is_running())
        return;

    if (uv_loop_alive(&m_loop))
        THROW_ON_LIBUV_ERROR(uv_async_send(m_async.get()));

    // join and wait for the thread to end.
    // Note: A thread that has finished executing code, but has not yet been joined
    //        is still considered an active thread of execution and is therefore joinable.
    if (m_thread.joinable())
        m_thread.join();
}

bool libuv_wrapper::is_running() const {
    return m_thread.joinable() && (uv_loop_alive(&m_loop) != 0);
}

void libuv_wrapper::start_libuv(libuv_on_start_cb_t on_start_cb, libuv_on_stop_cb_t on_stop_cb) {
    if (is_running())
        throw std::logic_error("this libuv loop is currently running");

    m_started_cb = on_start_cb;
    m_stopped_cb = on_stop_cb;

    /* setup UV loop */
    THROW_ON_LIBUV_ERROR(uv_loop_init(&m_loop));
    m_loop.data = this;

    /* call derived class operations */
    setup_libuv_operations();

    /* Setup handle for stopping the loop */
    m_async = std::make_unique<uv_async_t>();
    uv_async_init(&m_loop, m_async.get(), [](uv_async_t *handle) {
        ((libuv_wrapper *)handle->loop->data)->stop_libuv_operations();
        uv_stop(handle->loop);
    });

    m_thread = std::thread([&]() {
        if (m_started_cb)
            m_started_cb(this);
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
                &m_loop, [](uv_handle_s *handle, void *) { uv_close(handle, nullptr); }, nullptr);

            /* Check if there are any remaining callbacks*/
            if (uv_loop_close(&m_loop) != UV_EBUSY)
                break;
        }
        if (m_stopped_cb)
            m_stopped_cb(this);
    });
}

} // namespace sg
