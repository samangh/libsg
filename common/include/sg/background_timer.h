#pragma once

#include <sg/export/common.h>
#include "pimpl.h"
#include "accurate_sleeper.h"
#include "callback.h"

#include <atomic>
#include <exception>
#include <functional>
#include <shared_mutex>

namespace sg {

class SG_COMMON_EXPORT background_timer {
    class impl;
    sg::pimpl<impl> pimpl;

  public:
    CREATE_CALLBACK(started_cb_t, void, background_timer *)
    CREATE_CALLBACK(stopped_cb_t, void, background_timer *, std::exception_ptr)
    CREATE_CALLBACK(task_t, void, background_timer *)

    /* encapsulates a thread that runs a provided task regularly on a timer */
    background_timer(const task_t &task,
                     const started_cb_t &start_cb,
                     const stopped_cb_t &stopped_cb);
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

    /* interval in nanoseconds */
    uint64_t interval() const;
    void set_interval(uint32_t interval_ns,
                      sg::AccurateSleeper::Tragedy strategy = sg::AccurateSleeper::Tragedy::Auto);

    /* sets whether the time shouuld account for how long the action takes,
     * and remove that from the wait interval */
    void correct_for_task_delay(bool);

    /* pointer to exchange data with the task */
    void *data;

    /* mutex to use when using #data.This mutex is not used by
     * #background_timer, and is purely for teh users' convenience. */
    mutable std::shared_mutex data_mutex;
};

} // namespace sg
