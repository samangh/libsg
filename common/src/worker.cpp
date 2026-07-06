#include "sg/debug.h"
#include "sg/worker.h"

#include <cstdio>
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

worker::~worker() {
    /* Destroying the worker from its own thread can never be safe: even if the thread could be
     * "detached" from the class, it still uses member functions/variablles that would no longer
     * exist */
    if (m_thread_id.load(std::memory_order::acquire) == std::this_thread::get_id()) {
        std::fputs("sg::worker destroyed from within its own callback\n", stderr);
        std::terminate();
    }

    request_stop();
    wait_for_stop();
}

void worker::start(size_t stopAtIteration) {
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
        m_iterations_done.store(0, std::memory_order::relaxed);
        m_stop_at_iteration.store(stopAtIteration, std::memory_order::relaxed);
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

    const auto done = m_iterations_done.load(std::memory_order::relaxed);

    /* saturate so a huge request can't overflow into the no-stop sentinel */
    const auto target = iteration_count >= MAX_ITERATION_COUNT - done
                            ? MAX_ITERATION_COUNT - 1
                            : done + iteration_count;

    m_stop_at_iteration.store(target, std::memory_order::relaxed);
    notify();
}

void worker::wait_for_stop() {
    if (m_thread_id.load(std::memory_order::acquire) == std::this_thread::get_id())
        SG_THROW(std::logic_error,
                 "can't wait for notifiable_background_worker to stop from within itself");

    /* not: the worker thread must not block on this mutex! otherwise, we'll hae a deadlock if
     * another thread calls wait_for_stop() */
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
    return m_interval.load(std::memory_order::relaxed);
}

void worker::set_interval(std::chrono::nanoseconds interval) {
    m_interval.store(interval, std::memory_order::relaxed);
}
void worker::correct_for_task_delay(bool val) {
    m_correct_for_task_delay = val;
}

void worker::action(std::promise<void> start_promise) {
    /* needed for wait_for_stop() */
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
        while (!stop_requested()) {
            const auto iteration = m_iterations_done.fetch_add(1, std::memory_order::relaxed) + 1;

            /* calculate start time if needed */
            const auto t_start = m_correct_for_task_delay ? std::chrono::steady_clock::now()
                                                          : std::chrono::steady_clock::time_point{};
            m_callbacks.on_tick_callback.invoke(this);

            /* the "stop after iterations" target is checked after the tick (rather than doing
             * request_stop() before it) so that:
             *
             *  - a long-running tick that polls stop_requested() isn't told to stop early;
             *  - a request made during the tick is honoured;
             *  - we don't wait for the interval only to then stop */
            if (iteration >= m_stop_at_iteration.load(std::memory_order::relaxed))
                break;

            /* calculate wait time */
            const auto interval = m_interval.load(std::memory_order::relaxed);
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
