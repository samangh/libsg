#include "udaq/common/background_timer.h"

/* Note:
 *
 * A thread that has finished executing code, but has not yet been joined is
 * still considered an active thread of execution and is therefore joinable.
 */

namespace udaq::common {

background_timer::background_timer(const background_timer::task_t &task, const background_timer::started_cb_t &start_cb,
                                   const background_timer::stopped_cb_t &stopped_cb)
    : m_task(task), m_started_cb(start_cb), m_stopped_cb(stopped_cb) {}

background_timer::~background_timer() {
    request_stop();
    wait_for_stop();
}

void background_timer::start() {
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
    m_thread = std::thread(&background_timer::action, this);
}

void background_timer::wait_for_stop() {
    if (m_thread.get_id() != std::this_thread::get_id()) {
        /* called from different thread */
        if (m_thread.joinable())
            m_thread.join();
    } else {
        throw std::logic_error("can't wait for background_timer stop to stop from within itself");
    }
}

void background_timer::action() {
    if (m_started_cb)
        m_started_cb(this);

    while (!is_stop_requested()) {
        std::shared_lock lock(m_sleeper_mutex);
        try {
            if (m_correct_for_task_delay)
            {
                auto t = std::chrono::high_resolution_clock::now();
                m_task(this);
                m_sleeper.sleep(std::chrono::high_resolution_clock::now() - t);
            }
            else{
                m_task(this);
                m_sleeper.sleep();
            }
        } catch (...) {
            set_exception(std::current_exception());
            break;
        }
    }

    set_is_running(false);
    if (m_stopped_cb)
        m_stopped_cb(this, get_exception());
}

void background_timer::request_stop() {
    std::unique_lock lock(m_stop_mutex);
    m_stop_requested = true;
}

bool background_timer::is_stop_requested() const {
    std::shared_lock lock(m_stop_mutex);
    return m_stop_requested;
}

bool background_timer::has_exception() const
{
    std::lock_guard lock(m_exception_mutex);
    return (bool)m_exception;
}

void background_timer::set_exception(std::exception_ptr ptr)
{
    std::lock_guard lock(m_exception_mutex);
    m_exception = ptr;
}

std::exception_ptr background_timer::get_exception() const
{
    std::lock_guard lock(m_exception_mutex);
    return m_exception;
}

void background_timer::set_is_running(bool running) {
    std::unique_lock lock(m_running_mutex);
    m_is_running = running;
}

bool background_timer::is_running() const {
    std::shared_lock lock(m_running_mutex);
    return m_is_running;
}

uint64_t background_timer::interval() const {
    std::shared_lock lock(m_sleeper_mutex);
    return m_sleeper.interval();
}

void background_timer::set_interval(uint32_t interval_ns, AccurateSleeper::Strategy strategy) {
    std::unique_lock lock(m_sleeper_mutex);
    m_sleeper.set_interval(interval_ns, strategy);
}

void background_timer::correct_for_task_delay(bool correct)
{
    std::unique_lock lock(m_sleeper_mutex);
    m_correct_for_task_delay = correct;
}

} // namespace udaq::common
