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

namespace sg {

class COMMON_EXPORT background_task {
  public:
    typedef std::function<void(background_task *)> started_cb_t;
    typedef std::function<void(background_task *, std::exception_ptr error)> stopped_cb_t;
    typedef std::function<void(background_task *)> task_t;

    /* encapsulates a thread that runs a task in the background */
    background_task(const task_t &task, const started_cb_t &start_cb, const stopped_cb_t &stopped_cb);
    ~background_task();

    /* start */
    void start();

    /* request stop */
    void request_stop();

    /* request stop and wait for the thread to finish */
    void wait_for_stop();
    bool is_running() const;
    bool is_stop_requested() const;

    /* gets the exception, if the task threw an exception */
    std::exception_ptr get_exception() const;
    bool has_exception() const;

    /* pointer to exchange data with the task */
    void* data;
  private:
    task_t m_task;
    started_cb_t m_started_cb;
    stopped_cb_t m_stopped_cb;
    std::thread m_thread;

    mutable std::mutex m_exception_mutex;
    std::exception_ptr m_exception;

    mutable std::shared_mutex m_stop_mutex;
    bool m_stop_requested = false;

    mutable std::shared_mutex m_running_mutex;
    bool m_is_running = false;

    void action();
    void set_exception(std::exception_ptr ptr);
    void set_is_running(bool);
};

} // namespace sg
