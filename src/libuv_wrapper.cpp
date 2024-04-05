#include <sg/libuv_wrapper.h>

namespace sg {

libuv_wrapper::~libuv_wrapper() {
    stop();
}

void libuv_wrapper::stop() {
    stop_async();
    block_until_stopped();
}

bool libuv_wrapper::is_stopped() const noexcept
{
    return !m_uvloop_running;
}

bool libuv_wrapper::is_stopped_or_stopping() const noexcept
{
    return m_uvloop_stop_requested || is_stopped();
}

void libuv_wrapper::block_until_stopped() const
{
    /* need mutex as join() is not threadsafe */
    std::lock_guard lock(m_stopping_mutex);

    /* A thread that has finished executing code, but has not yet been
	 * joined is still considered an active thread of execution and is
	 * therefore joinable. */
    if (m_thread.joinable())
        m_thread.join();
}

void libuv_wrapper::start_libuv(libuv_on_start_cb_t on_start_cb, libuv_on_stop_cb_t on_stop_cb) {
    if (!is_stopped_or_stopping())
        throw std::logic_error("this libuv loop is currently running");

    /* If libuv loop is stopped (or process of stopping), but the thread not joined yet do it */
    block_until_stopped();
    m_uvloop_stop_requested = false;

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

        m_uvloop_running=true;

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

         m_uvloop_running=false;

        if (m_stopped_cb)
            m_stopped_cb(this);
    });
}

void libuv_wrapper::stop_async()
{
    if (!uv_loop_alive(&m_loop))
        return;

    auto stop_requested_previously = m_uvloop_stop_requested.exchange(true);
    if (stop_requested_previously)
        return;

    /* uv_async_send can throw error if we are in the process walking
     * the uv_loop callbacks, so ensure we do it only once */
    THROW_ON_LIBUV_ERROR(uv_async_send(m_async.get()));
}

} // namespace sg
