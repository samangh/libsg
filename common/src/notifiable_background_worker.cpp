#include "sg/notifiable_background_worker.h"

namespace sg {

notifiable_background_worker::notifiable_background_worker(std::chrono::nanoseconds interval_ns,
                                                           const callback_t &task,
                                                           const callback_t &start_cb,
                                                           const callback_t &stopped_cb)
    : m_interval(interval_ns),
      m_task(task),
      m_started_cb(start_cb),
      m_stopped_cb(stopped_cb) {}

notifiable_background_worker::~notifiable_background_worker() {
    request_stop();
    wait_for_stop();
}

void notifiable_background_worker::start_async() {
    if (is_running()) throw std::runtime_error("this worker is already running");

    /* if the thread has finished executing code,but has not been joined yet */
    wait_for_stop();

    try {
        m_result_promise = std::promise<void>();
        m_result_future = m_result_promise.get_future();

        /* we set m_is_running, as usually there is a delay before a
         * thread gets going and so the user can call the is_running
         * function duringt this delay */
        m_is_running.store(true);
        m_stop_requested.store(false);
        m_stop_after_interations.store(false);

        /* start */
        m_thread = std::thread(&notifiable_background_worker::action, this);
    } catch (...) {
        m_is_running.store(false);
        throw;
    }
}

void notifiable_background_worker::request_stop() {
    m_stop_requested.store(true, std::memory_order_release);
    notify();  // notify so that the loop immediately sees the stop event
}

void notifiable_background_worker::request_stop_after_iterations(size_t iteration_count){
    if(iteration_count==0){
        request_stop();
        return;
    }

    m_stop_after_interations_count.store(iteration_count, std::memory_order_release);
    m_stop_after_interations.store(true, std::memory_order_release);
    notify();
}

void notifiable_background_worker::wait_for_stop() {
    if (m_thread.get_id() != std::this_thread::get_id()) {
        /* called from different thread */
        if (m_thread.joinable()) m_thread.join();
    } else {
        throw std::logic_error(
            "can't wait for notifiable_background_timer to stop from within itself");
    }
}

bool notifiable_background_worker::is_running() const {
    return m_is_running;
}

bool notifiable_background_worker::stop_requested() const noexcept {
    // Even though we use our own stop atomic, jthread will still use the stop token if the
    // thread gets destructed whilst running
    return m_stop_requested.load(std::memory_order_acquire);
}

std::chrono::nanoseconds notifiable_background_worker::interval() const { return m_interval.load(std::memory_order_acquire);
}

void notifiable_background_worker::set_interval(std::chrono::nanoseconds interval) {
    m_interval.store(interval, std::memory_order_release);
}

void notifiable_background_worker::action() {
    m_semaphore_thread_started.release();

    /* catch latest exception */
    std::exception_ptr ex;

    /* we use this placeholder in case of "stop_after_iterations" request because:
     *
     *  - if we are meant to stop *after* this iteration, we don't want to do request_stop() before the iteration
     *    in case the worker action checks for it (e.g. if it's a long-lived operation).
     *
     *  - we don't want to do request_stop() after, because then we have waited for the interval for not reason
     */
    bool stop_after_iteration =false;

    try {        
        if (m_started_cb) m_started_cb(this);

        while (!stop_requested()) {
            if (m_stop_after_interations.load(std::memory_order_acquire))
                if(--m_stop_after_interations_count == 0)
                    stop_after_iteration=true;

            if (m_correct_for_task_delay) {
                auto t = std::chrono::high_resolution_clock::now();
                m_task(this);
                auto time_taken = std::chrono::high_resolution_clock::now() - t;
                if (stop_after_iteration)
                    break;
                std::ignore = m_semaphore_notifier.try_acquire_for(m_interval.load(std::memory_order_acquire) - time_taken);
            } else {
                m_task(this);
                if (stop_after_iteration)
                    break;
                std::ignore = m_semaphore_notifier.try_acquire_for(m_interval.load(std::memory_order_acquire));
            }

        }
    } catch (...) {
        ex = std::current_exception();
    }

    /* run m_stopped_cb even if theere was a previous exception */
    try {
        if (m_stopped_cb)
            m_stopped_cb(this);
    } catch (...) {
        ex = std::current_exception();
    }

    m_is_running.store(false);
    if (ex)
        m_result_promise.set_exception(ex);
    else
        m_result_promise.set_value();
}

void notifiable_background_worker::notify() { m_semaphore_notifier.release(); }

std::shared_future<void> notifiable_background_worker::future() const { return m_result_future; }

}  // namespace sg
