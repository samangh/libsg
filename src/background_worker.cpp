#include "udaq/common/background_worker.h"

/* Note:
 *
 * A thread that has finished executing code, but has not yet been joined is
 * still considered an active thread of execution and is therefore joinable.
 */

namespace udaq::common {

background_timer::background_timer(const background_timer::task_t &task,
                                     const background_timer::started_cb_t &start_cb,
                                     const background_timer::stopped_cb_t &stopped_cb)
    :m_task(task), m_started_cb(start_cb), m_stopped_cb(stopped_cb)
{
}

background_timer::~background_timer()
{
    wait_for_stop();
}

void background_timer::start()
{
    if (is_running())
        throw std::runtime_error("this worker is already running");

    /* of the thread has finished executing code,but has not been joined yet */
    wait_for_stop();

    m_stop_requested = false;
    m_thread = std::thread(&background_timer::action, this);
}


void background_timer::wait_for_stop()
{
    request_stop();
    if (m_thread.get_id() != std::this_thread::get_id())
    {
        /* called from different thread */
        if (m_thread.joinable())
            m_thread.join();
    } else {
        throw std::logic_error("can't wait for background stop to stop within the worker itself");
    }
}

void background_timer::action()
{
    set_is_running(true);
    if (m_started_cb)
        m_started_cb(this);

    std::exception_ptr ex;
    while (!is_stop_requested())
    {
        try{
            m_task(this);
            std::lock_guard<std::mutex> lock(m_sleeper_mutex);
            m_sleeper.sleep();
        } catch (...)
        {
            ex= std::current_exception();
            break;
        }
    }

    set_is_running(false);
    if (m_stopped_cb)
        m_stopped_cb(this, ex);
}

void background_timer::request_stop()
{
    std::lock_guard lock(m_stop_mutex);
    m_stop_requested = true;
}

bool background_timer::is_stop_requested()
{
    std::lock_guard lock(m_stop_mutex);
    return  m_stop_requested;
}

void background_timer::set_is_running(bool running)
{
    std::unique_lock lock(m_running_mutex);
    m_is_running =running;
}

bool background_timer::is_running() const
{
    std::shared_lock lock(m_running_mutex);
    return m_is_running;
}

uint64_t background_timer::get_interval() const
{
    std::lock_guard<std::mutex>  lock(m_sleeper_mutex);
    return m_sleeper.interval();
}

void background_timer::set_interval(uint32_t interval_ns, AccurateSleeper::Strategy strategy)
{
    std::lock_guard<std::mutex>  lock(m_sleeper_mutex);
    m_sleeper.set_interval(interval_ns, strategy);
}

}
