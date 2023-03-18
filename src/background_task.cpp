#include "udaq/common/background_task.h"

/* Note:
 *
 * A thread that has finished executing code, but has not yet been joined is
 * still considered an active thread of execution and is therefore joinable.
 */

namespace sg {

background_task::background_task(const background_task::task_t &task, const background_task::started_cb_t &start_cb,
                                   const background_task::stopped_cb_t &stopped_cb)
    : m_task(task), m_started_cb(start_cb), m_stopped_cb(stopped_cb) {}

background_task::~background_task() {
    request_stop();
    wait_for_stop();
}

void background_task::start() {
    if (is_running())
        throw std::runtime_error("this worker is already running");

    /* if the thread has finished executing code,but has not been joined yet */
    wait_for_stop();

    /* we set these here, as usually there is a delay before a thread gets going
     * and so the user call call functions that use these variables before they are
     * set in the new thread */

    m_stop_requested = false;
    set_exception(nullptr);
    set_is_running(true);
    m_thread = std::thread(&background_task::action, this);
}

void background_task::wait_for_stop() {
    if (m_thread.get_id() != std::this_thread::get_id()) {
        /* called from different thread */
        if (m_thread.joinable())
            m_thread.join();
    } else {
        throw std::logic_error("can't wait for background_task stop to stop from within itself");
    }
}

void background_task::action() {
    if (m_started_cb)
        m_started_cb(this);

    if (!is_stop_requested())
    {
        try {
                m_task(this);
        } catch (...) {
            set_exception(std::current_exception());
        }
    }

    set_is_running(false);
    if (m_stopped_cb)
        m_stopped_cb(this, get_exception());
}

void background_task::request_stop() {
    std::unique_lock lock(m_stop_mutex);
    m_stop_requested = true;
}

bool background_task::is_stop_requested() const {
    std::shared_lock lock(m_stop_mutex);
    return m_stop_requested;
}

bool background_task::has_exception() const
{
    std::lock_guard lock(m_exception_mutex);
    return (bool)m_exception;
}

void background_task::set_exception(std::exception_ptr ptr)
{
    std::lock_guard lock(m_exception_mutex);
    m_exception = ptr;
}

std::exception_ptr background_task::get_exception() const
{
    std::lock_guard lock(m_exception_mutex);
    return m_exception;
}

void background_task::set_is_running(bool running) {
    std::unique_lock lock(m_running_mutex);
    m_is_running = running;
}

bool background_task::is_running() const {
    std::shared_lock lock(m_running_mutex);
    return m_is_running;
}


} // namespace sg
