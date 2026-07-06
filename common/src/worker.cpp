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
}

void worker::start() {
    if (auto state_ = state_t::stopped; !m_state.compare_exchange_strong(state_, state_t::running,
                                                            std::memory_order::acq_rel,
                                                            std::memory_order::acquire))
            SG_THROW(std::runtime_error, "this worker is already running");

    /* if the thread has finished executing code,but has not been joined yet */
    wait_for_stop();

    try {
        std::promise<void> start_promise;
        auto start_future = start_promise.get_future();

        /* the future returned by packaged_task stores any exception thrown by action() */
        std::packaged_task<void()> task(
            [this, p = std::move(start_promise)]() mutable { action(std::move(p)); });
        set_result_future(task.get_future().share());

        m_stop_requested.store(false);
        m_stop_after_iterations_count.store(0);
        m_checked_future.store(false);

        /* discard notifications left over from a previous run */
        while (m_notify_sem.try_acquire()) {}

        /* start */
        try {
            std::lock_guard lock(m_join_mutex);
            m_thread = std::thread(std::move(task));
        } catch (...) {
            /* the task never ran, so its future would report broken_promise;
             * invalidate it instead */
            set_result_future({});
            throw;
        }

        /* throws if the start callback threw; action() marks the worker as
         * stopped before delivering that exception */
        start_future.get();
    } catch (...) {
        m_state.store(state_t::stopped, std::memory_order::release);
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
    /* deliberately checked before taking m_join_mutex: the worker thread must
     * not block on the mutex, as it may be held by a thread join()ing us */
    if (m_thread_id.load(std::memory_order::acquire) == std::this_thread::get_id())
        SG_THROW(std::logic_error,
                 "can't wait for notifiable_background_worker to stop from within itself");

    std::lock_guard lock(m_join_mutex);

    /* called from different thread */
    if (m_thread.joinable()) {
        m_thread.join();
        m_thread_id.store({}, std::memory_order::release);
    }
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

void worker::action(std::promise<void> start_promise) {
    /* published before any callback runs, so the self-wait check in
     * wait_for_stop() works even from inside the start callback */
    m_thread_id.store(std::this_thread::get_id(), std::memory_order::release);

    try {
        if (m_callbacks.on_start_callback)
            m_callbacks.on_start_callback.invoke(this);
        start_promise.set_value();
    } catch(...) {
        m_state.store(state_t::stopped, std::memory_order::release);
        start_promise.set_exception(std::current_exception());
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
        /* catch exception as we want to run the completion callback even if there was an error */
        ex = std::current_exception();
    }

    m_state.store(state_t::stopped, std::memory_order::release);

    /* run m_stopped_cb even if there was a previous exception */
    if (m_callbacks.on_stop_callback)
        m_callbacks.on_stop_callback.invoke(this);

    /* rethrown exceptions are stored in m_result_future by the packaged_task */
    if (ex)
        std::rethrow_exception(ex);
}

void worker::wait_until_next_tick(std::chrono::nanoseconds duration) {
    if (duration > std::chrono::nanoseconds::zero())
        m_notify_sem.try_acquire_for(duration);

    /* drain any extra releases so multiple notify() calls coalesce
     * into at most one early tick */
    while (m_notify_sem.try_acquire()) {}
}

void worker::notify() {
    m_notify_sem.release();
}

std::shared_future<void> worker::future() const {
    std::lock_guard lock(m_future_mutex);
    return m_result_future;
}

void worker::set_result_future(std::shared_future<void> fut) {
    std::lock_guard lock(m_future_mutex);
    m_result_future = std::move(fut);
}

void worker::future_get_once(){
    if (!m_checked_future.exchange(true))
        if (const auto fut=future(); fut.valid())
            fut.get();
}

}  // namespace sg
