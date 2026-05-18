#include "sg/debug.h"
#include "sg/worker.h"

#include <utility>

namespace sg {

worker::worker(std::chrono::nanoseconds interval_ns,
                                                           on_tick_callback_t task,
                                                           on_start_callback_t start_cb,
                                                           on_stop_callback_t stopped_cb)
    : worker(interval_ns, callbacks_t{.on_start_callback = std::move(start_cb),
                                                            .on_tick_callback  = std::move(task),
                                                            .on_stop_callback  = std::move(stopped_cb)}) {};

worker::worker(std::chrono::nanoseconds intervalNs,
                                                           callbacks_t callbacks)
    : m_interval(intervalNs),
      m_callbacks(std::move(callbacks)) {}

worker::~worker() noexcept(false) {
    request_stop();
    wait_for_stop();

    /* throw error if no one else has checked for it! */
    future_get_once();
}

void worker::start() {
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
        m_thread = std::thread(&worker::action, this);

        m_start_promise.get_future().get();
    } catch (...) {
        m_state.store(state_t::stopped, std::memory_order::release);
        m_result_promise.set_value();
        throw;
    }
}

void worker::request_stop() {
    m_stop_requested.store(true, std::memory_order_release);
    notify();  // notify so that the loop immediately sees the stop event
}

void worker::request_stop_after_iterations(size_t iteration_count){
    if(iteration_count==0){
        request_stop();
        return;
    }

    m_stop_after_iterations_count.store(iteration_count, std::memory_order_release);
    notify();
}

void worker::wait_for_stop() {
    if (m_thread.get_id() == std::this_thread::get_id())
        SG_THROW(std::logic_error,
                 "can't wait for notifiable_background_worker to stop from within itself");

    // mutex needed because of the gap between .joinable and .join
    std::lock_guard lock(m_join_mutex);

    /* called from different thread */
    if (m_thread.joinable())
        m_thread.join();
}

bool worker::is_running() const {
    return m_state.load(std::memory_order::acquire) == state_t::running;
}

bool worker::stop_requested() const noexcept {
    return m_stop_requested.load(std::memory_order::acquire);
}

std::chrono::nanoseconds worker::interval() const {
    return m_interval.load(std::memory_order::acquire);
}

void worker::set_interval(std::chrono::nanoseconds interval) {
    m_interval.store(interval, std::memory_order_release);
}
void worker::correct_for_task_delay(bool val) {
    m_correct_for_task_delay = val;
}

void worker::action() {
    try {
        if (m_callbacks.on_start_callback)
            m_callbacks.on_start_callback.invoke(this);
        m_start_promise.set_value();
    } catch(...) {
        m_state.store(state_t::stopped, std::memory_order::release);
        m_start_promise.set_exception(std::current_exception());
        return;
    }

    /* catch latest exception */
    std::exception_ptr ex;

    try {
        /* we use this placeholder in case of "stop_after_iterations" request because:
         *
         *  - if we are meant to stop *after* this iteration, we don't want to do request_stop()
         * before the iteration in case the worker action checks for it (e.g. if it's a long-lived
         * operation).
         *
         *  - we don't want to do request_stop() after, because then we have waited for the interval
         * for no reason
         */
        bool stop_after_iteration = false;

        while (!stop_requested()) {
            if (m_stop_after_iterations_count.load(std::memory_order::acquire))
                if (m_stop_after_iterations_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    stop_after_iteration = true;

            /* calculate start time if needed */
            const auto t_start = m_correct_for_task_delay ? std::chrono::steady_clock::now()
                                                          : std::chrono::steady_clock::time_point{};
            m_callbacks.on_tick_callback.invoke(this);
            if (stop_after_iteration)
                break;

            /* calculate wait time */
            const auto interval = m_interval.load(std::memory_order::acquire);
            const auto wait_duration = m_correct_for_task_delay
                ? interval - (std::chrono::steady_clock::now() - t_start)
                : interval;

            wait_until_next_tick(wait_duration);
        }
    } catch (...) {
        ex = std::current_exception();
    }

    m_state.store(state_t::stopped, std::memory_order::release);

    /* run m_stopped_cb even if there was a previous exception */
    try {
        if (m_callbacks.on_stop_callback)
            m_callbacks.on_stop_callback.invoke(this);
    } catch (...) {
        ex = std::current_exception();
    }

    if (ex)
        m_result_promise.set_exception(ex);
    else
        m_result_promise.set_value();

}

void worker::wait_until_next_tick(std::chrono::nanoseconds duration) {
    std::unique_lock lock(m_notify_mutex);
    if (duration > std::chrono::nanoseconds::zero())
        m_notify_cv.wait_for(lock, duration, [this] { return m_notified; });
    m_notified = false;
}

void worker::notify() {
    {
        std::lock_guard lock(m_notify_mutex);
        m_notified = true;
    }
    m_notify_cv.notify_one();
}

std::shared_future<void> worker::future() const { return m_result_future; }

void worker::future_get_once(){
    if (!m_checked_future.exchange(true))
        if (const auto fut=future(); fut.valid())
            fut.get();
}

}  // namespace sg
