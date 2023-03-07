#pragma once

#include "udaq/export/common.h"

#include "accurate_sleeper.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>

namespace udaq::common {

class COMMON_EXPORT background_timer {
  public:
    typedef std::function<void(background_timer *)> started_cb_t;
    typedef std::function<void(background_timer *, std::exception_ptr error)> stopped_cb_t;
    typedef std::function<void(background_timer *)> task_t;

    /* encapsulates a thread that runs a provided task regualrly on a timer */
    background_timer(const task_t &task, const started_cb_t &start_cb, const stopped_cb_t &stopped_cb);
    ~background_timer();

    /* start */
    void start();

    /* request stop */
    void request_stop();

    /* request stop and wait for the thread to finish */
    void wait_for_stop();
    bool is_running() const;
    bool is_stop_requested() const;

    /* gets the exception, if the task threw an exception*/
    std::exception_ptr get_exception() const;
    bool has_exception() const;

    /* gets the interval in seconds */
    uint64_t interval() const;
    void set_interval(uint32_t interval_ns,
                      udaq::common::AccurateSleeper::Strategy strategy = udaq::common::AccurateSleeper::Strategy::Auto);

    /* pointer to exchange data with the task */
    void* data;

    /* mutex to use when using #data.This mutex is not used by
     * #background_timer, and is purely for teh users' convenience. */
    mutable std::shared_mutex data_mutex;

    /* sets whether the time shouuld account for how long the action takes,
     * and remove that from the wait interval */
    void correct_for_task_delay(bool);
  private:
    task_t m_task;
    started_cb_t m_started_cb;
    stopped_cb_t m_stopped_cb;
    std::thread m_thread;

    mutable std::mutex m_exception_mutex;
    std::exception_ptr m_exception;

    mutable std::shared_mutex m_stop_mutex;
    bool m_stop_requested = false;

    mutable std::shared_mutex m_sleeper_mutex;
    AccurateSleeper m_sleeper;
    bool m_correct_for_task_delay =false;

    mutable std::shared_mutex m_running_mutex;
    bool m_is_running = false;

    void action();
    void set_exception(std::exception_ptr ptr);
    void set_is_running(bool);
};

} // namespace udaq::common
