#pragma once

#include <sg/export/common.h>
#include "callback.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <semaphore>
#include <thread>
#include <future>

namespace sg {

/**
 * @brief The notifiable_background_worker class represents a background
 *        worker, where the timer "tick" action can be triggered on
 *        demand by sending a "notify" signal.
 */
class SG_COMMON_EXPORT worker final {
  public:
    CREATE_CALLBACK(on_start_callback_t, void(worker *))
    CREATE_CALLBACK(on_stop_callback_t, void(worker *))
    CREATE_CALLBACK(on_tick_callback_t, void(worker *))

    /** callbacks for the worker
     * Notes:
     *
     *   - all callbacks are done on the worker thread.
     *   - stop callback WILL NOT be called if is an error in the start callback.
     *   - if there is an exception in the start callback, the start() function will throw
     *   - exceptions in the task or stop callbacks are stored in future.
     */
    struct callbacks_t {
        on_start_callback_t on_start_callback{nullptr};
        on_tick_callback_t on_tick_callback{nullptr};
        on_stop_callback_t on_stop_callback{nullptr};
    };

    worker(std::chrono::nanoseconds interval_ns,
                                 on_tick_callback_t task,
                                 on_start_callback_t start_cb,
                                 on_stop_callback_t stopped_cb);
    worker(std::chrono::nanoseconds intervalNs, callbacks_t callbacks);

    ~worker() noexcept(false);


    /**
     * @brief start synchronously starts the thread
     *
     * @throws if the thread could not be started for any reason.
     */
    void start();

    /**
     * @brief request_stop asynchronously stops the thread.
     *        To check if the thread has stopped completely, run @see wait_for_stop().
     *
     *        Is thread safe.
     */
    void request_stop();

    /**
     * Request stops after the provider number of iterations have been done. This should not be
     * called concurrently by multiple threads.
     *
     * Note if this is called inside the worker then it does NOT include the current iteration.
     */
    void request_stop_after_iterations(size_t iteration_count);

    /**
     * @brief wait_for_stop blocks until the thread has stopped.
     *        Note: this does not initiate a stop request, you have to do that separately.
     */
    void wait_for_stop();

    bool is_running() const;
    bool stop_requested() const noexcept;

    /**
     * @brief causes the timer to tick immediately.
     */
    void notify();

    std::shared_future<void> future() const;

    /**
     * @brief future_get_once will wait until the worker is finished.
     *
     * If this the first time this function is called, it will check the future() result and so
     * will throw if it contains an exception
     *
     * If this is not the first time, the future() is not checked.
     */
    void future_get_once();

    /** Returns the interval. Is thread safe. */
    std::chrono::nanoseconds interval() const;

    /**
     * Sets the interval. Is thread safe.
     *
     * If the new interval is shorter than the old one, consider calling @code notify()@endcode
     * after this.
     */
    void set_interval(std::chrono::nanoseconds interval);

    /**
     *  correct_for_task_delay sets whether the time should account
     *  for how long the action takes, and remove that from the wait
     *  interval.
     *
     *  This must be set before starting the worker.
     */
    void correct_for_task_delay(bool);

  private:
    enum class state_t { running, stopped };

    /* needs to be atomic because we can change it */
    std::atomic<std::chrono::nanoseconds> m_interval;

    std::atomic<state_t> m_state {state_t::stopped};

    std::atomic<size_t> m_stop_after_iterations_count{0};
    std::atomic<bool> m_checked_future;

    // We don't use `std::jthread` because:
    //
    //  * in MSVC/MSYS it seemed that the stop_token is buggy/not thread safe;
    //  * in some implementations the stop_token uses relaxed memory ordering,
    //    which is not thread safe

    mutable std::mutex m_join_mutex;
    std::thread m_thread;
    std::atomic<bool> m_stop_requested;

    std::counting_semaphore<> m_notify_sem{0};

    callbacks_t m_callbacks;

    std::promise<void> m_start_promise;
    std::promise<void> m_result_promise;
    std::shared_future<void> m_result_future;

    bool m_correct_for_task_delay = false;

    void action();
    void wait_until_next_tick(std::chrono::nanoseconds duration);
};

} // namespace sg
