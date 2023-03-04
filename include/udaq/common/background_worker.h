#pragma once

#include "udaq/export/common.h"

#include "accurate_sleeper.h"

#include <functional>
#include <exception>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

namespace udaq::common {

class COMMON_EXPORT background_worker {
public:
    typedef std::function<void(background_worker*)> started_cb_t;
    typedef std::function<void(background_worker*, std::exception_ptr error)> stopped_cb_t;
    typedef std::function<void(background_worker*)> task_t;

    background_worker(const task_t& task, const started_cb_t& start_cb, const stopped_cb_t& stopped_cb);
    ~background_worker();

    void start();
    void stop();
    void wait_for_stop();

    bool is_running();
    void set_interval(uint32_t interval_ns, udaq::common::AccurateSleeper::Strategy strategy);

    std::shared_ptr<void> data;
private:
    task_t m_task;
    started_cb_t m_started_cb;
    stopped_cb_t m_stopped_cb;
    std::thread m_thread;

    std::mutex m_stop_mutex;
    bool m_stop_requested;

    std::mutex m_sleeper_mutex;
    AccurateSleeper m_sleeper;

    std::mutex m_running_mutex;
    bool m_is_running;

    void action();
    bool is_stop_requested();

    void set_is_running(bool);
};

}
