#include "udaq/common/background_worker.h"

namespace udaq::common {



background_worker::background_worker(const background_worker::task_t &task, const background_worker::started_cb_t &start_cb, const background_worker::stopped_cb_t &stopped_cb)
    :m_task(task), m_started_cb(start_cb), m_stopped_cb(stopped_cb)
{
}

background_worker::~background_worker()
{
    if (is_running())
        wait_for_stop();
}

void background_worker::start()
{
    m_stop_requested = false;
    m_thread = std::thread(&background_worker::action, this);
}

void background_worker::stop()
{
    std::lock_guard<std::mutex>  lock(m_stop_mutex);
    m_stop_requested = true;
}

void background_worker::wait_for_stop()
{
    stop();
    if (m_thread.get_id() != std::this_thread::get_id())
    {
        /* called from different thread */
        if (m_thread.joinable())
            m_thread.join();
    } else
    {
        throw std::logic_error("can't wait for background worker to stop witin the worker itself");
    }
}

void background_worker::action()
{
    set_is_running(true);
    m_started_cb(this);

    std::exception_ptr ex;

    while (!is_stop_requested())
    {
        try{
            m_task(this);
            std::lock_guard<std::mutex>  lock(m_sleeper_mutex);
            m_sleeper.sleep();

        } catch (...)
        {
            ex= std::current_exception();
            break;
        }
    }

    set_is_running(false);
    m_stopped_cb(this, ex);
}

bool background_worker::is_stop_requested()
{
    std::lock_guard<std::mutex>  lock(m_stop_mutex);
    return  m_stop_requested;
}

void background_worker::set_is_running(bool running)
{
    std::lock_guard<std::mutex>  lock(m_running_mutex);
    m_is_running =running;
}

void background_worker::set_interval(uint32_t interval_ns, AccurateSleeper::Strategy strategy)
{
    std::lock_guard<std::mutex>  lock(m_sleeper_mutex);
    m_sleeper.set_interval(interval_ns, strategy);
}

bool background_worker::is_running()
{
    /* called from outside the action thread */
    if (m_thread.get_id() != std::this_thread::get_id())
        return m_thread.joinable();

    std::lock_guard<std::mutex>  lock(m_sleeper_mutex);
    return m_is_running;
}

}
