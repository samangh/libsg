#include "sg/notifiable_background_worker.h"

#include "sg/debug.h"

#include <utility>

namespace sg {

notifiable_background_worker::notifiable_background_worker(std::chrono::nanoseconds interval_ns,
                                                           on_tick_callback_t task,
                                                           on_start_callback_t start_cb,
                                                           on_stop_callback_t stopped_cb)
    : m_interval(interval_ns),
      m_task(std::move(task)),
      m_started_cb(std::move(start_cb)),
      m_stopped_cb(std::move(stopped_cb)) {}

notifiable_background_worker::~notifiable_background_worker() noexcept(false) {
    request_stop();
    wait_for_stop();

    /* throw error if no one else has checked for it! */
    future_get_once();
}

void notifiable_background_worker::start() {
    if (auto state_ = state_t::stopped; !m_state.compare_exchange_strong(state_, state_t::running,
                                                            std::memory_order::acq_rel,
                                                            std::memory_order::acquire))
            SG_THROW(std::runtime_error, "this worker is already running");

    /* if the thread has finished executing code,but has not been joined yet */
    wait_for_stop();

    try {
        m_result_promise = std::promise<void>();
        m_start_promise = std::promise<void>();
        m_result_future = m_result_promise.get_future();

        m_stop_requested.store(false);
        m_stop_after_iterations_count.store(0);
        m_checked_future.store(false);
        m_notified = false;

        /* start */
        m_thread = std::thread(&notifiable_background_worker::action, this);

        m_start_promise.get_future().get();
    } catch (...) {
        m_state.store(state_t::stopped, std::memory_order::release);
        m_result_promise.set_value();
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

    m_stop_after_iterations_count.store(iteration_count, std::memory_order_release);
    notify();
}

void notifiable_background_worker::wait_for_stop() {
    if (m_thread.get_id() == std::this_thread::get_id())
        SG_THROW(std::logic_error,
                 "can't wait for notifiable_background_worker to stop from within itself");

    // mutex needed because of the gap between .joinable and .join
    std::lock_guard lock(m_join_mutex);

    /* called from different thread */
    if (m_thread.joinable())
        m_thread.join();
}

bool notifiable_background_worker::is_running() const {
    return m_state.load(std::memory_order::acquire) == state_t::running;
}

bool notifiable_background_worker::stop_requested() const noexcept {
    return m_stop_requested.load(std::memory_order_acquire);
}

std::chrono::nanoseconds notifiable_background_worker::interval() const {
    return m_interval.load(std::memory_order_acquire);
}

void notifiable_background_worker::set_interval(std::chrono::nanoseconds interval) {
    m_interval.store(interval, std::memory_order_release);
}
void notifiable_background_worker::correct_for_task_delay(bool val) {
    m_correct_for_task_delay = val;
}

void notifiable_background_worker::action() {
    try {
        if (m_started_cb)
            m_started_cb.invoke(this);
        m_start_promise.set_value();
    } catch(...) {
        m_start_promise.set_exception(std::current_exception());
        request_stop();
        return;
    }

    /* catch latest exception */
    std::exception_ptr ex;

    /* we use this placeholder in case of "stop_after_iterations" request because:
     *
     *  - if we are meant to stop *after* this iteration, we don't want to do request_stop() before the iteration
     *    in case the worker action checks for it (e.g. if it's a long-lived operation).
     *
     *  - we don't want to do request_stop() after, because then we have waited for the interval for no reason
     */
    bool stop_after_iteration =false;

    try {
        while (!stop_requested()) {
            if (m_stop_after_iterations_count.load(std::memory_order_acquire))
                if(m_stop_after_iterations_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    stop_after_iteration=true;

            if (m_correct_for_task_delay) {
                const auto t = std::chrono::steady_clock::now();
                m_task.invoke(this);
                if (stop_after_iteration)
                    break;
                const auto remaining = m_interval.load(std::memory_order_acquire)
                                     - (std::chrono::steady_clock::now() - t);
                if (remaining > std::chrono::nanoseconds::zero()) {
                    std::unique_lock lock(m_notify_mutex);
                    m_notify_cv.wait_for(lock, remaining, [this] { return m_notified; });
                    m_notified = false;
                }
                // else: task overran the interval — fall through and tick immediately.
            } else {
                m_task.invoke(this);
                if (stop_after_iteration)
                    break;
                std::unique_lock lock(m_notify_mutex);
                m_notify_cv.wait_for(lock, m_interval.load(std::memory_order_acquire),
                                     [this] { return m_notified; });
                m_notified = false;
            }

        }
    } catch (...) {
        ex = std::current_exception();
    }

    m_state.store(state_t::stopped, std::memory_order::release);

    /* run m_stopped_cb even if there was a previous exception */
    try {
        if (m_stopped_cb)
            m_stopped_cb.invoke(this);
    } catch (...) {
        ex = std::current_exception();
    }

    if (ex)
        m_result_promise.set_exception(ex);
    else
        m_result_promise.set_value();

}

void notifiable_background_worker::notify() {
    {
        std::lock_guard lock(m_notify_mutex);
        m_notified = true;
    }
    m_notify_cv.notify_one();
}

std::shared_future<void> notifiable_background_worker::future() const { return m_result_future; }

void notifiable_background_worker::future_get_once(){
    if (!m_checked_future.exchange(true))
        if (const auto fut=future(); fut.valid())
            fut.get();
}

}  // namespace sg
