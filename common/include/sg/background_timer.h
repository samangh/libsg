#pragma once

#include "pimpl.h"
#include "sg/export/sg_common.h"

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

class SG_COMMON_EXPORT sequential_worker {
    std::thread m_thread;
    std::stop_source m_stop_source;

    std::mutex m_lock;
    std::vector<std::function<void(std::stop_token)>> m_functions;

    std::jthread background_timer;

    template <typename Callable, typename... Args>
    std::thread store_function(Callable&& cb, Args&&... args) {
        std::function<void(std::stop_token)> func;
        if constexpr (std::is_invocable_v<Callable, std::stop_token, Args...>)
            func = [&cb, &args...](std::stop_token token) {
                std::invoke(
                    std::forward<decltype(cb)>(cb), token, std::forward<decltype(args)>(args)...);
            };
        else
            func = [&cb, &args...](std::stop_token) {
                std::invoke(
                    std::forward<decltype(cb)>(cb), std::forward<decltype(args)>(args)...);
            };

        std::lock_guard lock(m_lock);
        m_functions.emplace_back(std::move(func));
    }

public:
    sequential_worker() {}

     template <typename Callable,
               typename... Args,
               typename = ::std::enable_if_t<!::std::is_same_v<::std::decay_t<Callable>, sequential_worker>>>
     void add(Callable&& cb, Args&&... args)
     {
         store_function(std::forward<Callable>(cb), std::forward<Args>(args)...);
     }

     template <typename Callable,
               typename... Args,
               typename = ::std::enable_if_t<!::std::is_same_v<::std::decay_t<Callable>, sequential_worker>>,
               typename = ::std::enable_if_t<std::is_invocable_v<Callable, sequential_worker*, Args...>>>
     void add2(Callable&& cb, Args&&... args)
     {
         //store_function(std::forward<Callable>(cb), std::forward<Args>(args)...);
     }

};

class SG_COMMON_EXPORT background_timer {
    class impl;
    sg::pimpl<impl> pimpl;

  public:
    typedef std::function<void(background_timer *)> started_cb_t;
    typedef std::function<void(background_timer *, std::exception_ptr error)> stopped_cb_t;
    typedef std::function<void(background_timer *)> task_t;

    /* encapsulates a thread that runs a provided task regualrly on a timer */
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

    /* gets the interval in seconds */
    uint64_t interval() const;
    void set_interval(uint32_t interval_ns,
                      sg::AccurateSleeper::Strategy strategy = sg::AccurateSleeper::Strategy::Auto);

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
