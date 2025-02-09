#pragma once

#include "sg/export/common.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <semaphore>
#include <thread>

#include <future>

namespace sg {

/**
 * @brief The notifiable_background_worker class represents a background
 *        worker, where the timer "tick" action can be trigerred on
 *        demand by sending a "notify" signal.
 */
class SG_COMMON_EXPORT notifiable_background_worker {
  public:
    typedef std::function<void(notifiable_background_worker *)> callback_t;

    /**
     * @brief notifiable_background_worker
     *
     *        Notes:
     *         * all callbacks are done on the worker thread.
     *         * all exceptions in the start or task callbacks are
     *           stored in future.
     *         * stop callback WILL be called even if there is an error
     *           in the start callback.
     *
     * @param interval_ns
     * @param task is the callback/action that is done on every iteration.
     * @param start_cb is the callback/action that is called BEFORE the worker starts
     * @param stopped_cb s the callback/action that is called AFTER the worker starts
     */
    notifiable_background_worker(std::chrono::nanoseconds interval_ns,
                                 const callback_t &task,
                                 const callback_t &start_cb,
                                 const callback_t &stopped_cb);
    virtual ~notifiable_background_worker();

    /**
     * @brief start_async asynchronously starts the worker.
     *        Note that the acutal thread may not have started when this call returns.
     *
     * @throws if the thread could not be started for any reason.
     */
    void start_async();

    /**
     * @brief start syncrhonously starts the thread
     *
     * @throws if the thread could not be started for any reason.
     */
    void start() {
        start_async();
        m_semaphore_thread_started.acquire();
    }

    /**
     * @brief request_stop asynchronously stops the thread.
     *        To check if the thread has stopped completely, run @see wait_for_stop().
     *
     *        Is thread safe.
     */
    void request_stop();

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

    /* interval in nanoseconds */
    /**
     * @brief returns the interval
     *        Is thread safe
     * @return
     */
    std::chrono::nanoseconds interval() const;
    /**
     * @brief set_interval sets the interval
     *        Is thread safe.
     * @param interval
     */
    void set_interval(std::chrono::nanoseconds interval);

    /**
     * @brief correct_for_task_delay sets whether the time shouuld account
     *        for how long the actiontakes, and remove that from the wait
     *        interval.
     *
     *        Is NOT thread safe. Set before starting the thread.
     */
    void correct_for_task_delay(bool);

    /**
     * @brief mutex for use by the user as neeeded. This is not used by
     *        the worker. Thi is purely for the users' convenience.
     */
    mutable std::shared_mutex data_mutex;
    /**
     * @brief opaque pointer for use by the user as neeeded. This is not used by
     *        the worker. Thi is purely for the users' convenience.
     */
    void *data;

  private:
    /* needs to be atomic because we can change it */
    std::atomic<std::chrono::nanoseconds> m_interval;

    // We don't use `std::jthread` because:
    //
    //  * in MSVC/MSYS it seemed that the stop_token is buggy/not thread safe;
    //  * in some implementations the stop_token uses relaxed memory ordering,
    //    which is not thead safe

    std::thread m_thread;
    std::atomic<bool> m_is_running;
    std::atomic<bool> m_stop_requested;

    std::binary_semaphore m_semaphore_notifier {0};
    std::binary_semaphore m_semaphore_thread_started {0};

    callback_t m_task;
    callback_t m_started_cb;
    callback_t m_stopped_cb;

    std::promise<void> m_result_promise;
    std::shared_future<void> m_result_future;

    bool m_correct_for_task_delay = false;

    void action();
};

} // namespace sg
