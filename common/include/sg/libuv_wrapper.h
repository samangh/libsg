#pragma once

#include "weak_function.h"
#include "sg/export/sg_common.h"

#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <uv.h>

#define THROW_ON_LIBUV_ERROR(err)                                                                  \
    do {                                                                                           \
        if (err < 0)                                                                               \
            throw std::runtime_error(uv_strerror(err));                                            \
    } while (0);

namespace sg {

/* wrapper around libuv, helps with starting and stopping it properly */
class SG_COMMON_EXPORT libuv_wrapper {
  public:
    enum class wrapup_result {
        rerun_uv_loop,
        stop_uv_loop
    };

    typedef weak_function<sg::enable_lifetime_indicator::item_type, void, libuv_wrapper *> cb_t;
    typedef weak_function<sg::enable_lifetime_indicator::item_type, wrapup_result, libuv_wrapper *> cb_wrapup_t;

    /* add a callback to be called just before the uv_loop is started.
     *
     * Callback is called only once. You must add it if you need it to
     * be called on futher starts/stops. */
    void add_on_loop_started_cb(cb_t);

    /* add a callback to be called just after the uv_loop is stopped.
     *
     * Callback is called only once. You must add it if you need it to
     * be called on further starts/stops. */
    void add_on_stopped_cb(cb_t);

    /**
     * @brief runs the given set task, then starts the libuv loop if
     * needed
     *
     * @tparam setup_action is called as part of the setup process. This
     * will always be called after the loop is initialised. This action
     * is done in the calling thread.
     *
     * @tparam wrapup_action is called right before the uv_loop is
     * stopped. This action is done ithe libuv event loop.
     */
    size_t start_task(cb_t setup_action, cb_wrapup_t wrapup_action);

    /* Stops the libuv loop syncrhonously.
     *
     * This is thread-safe, but is NOT async-signal-safe. This should
     * not be called from a signal handler or from the uv loop
     * thread. Instead call abort_uv_loop() if you need to abort from
     * within a callback */
    void stop();

    /* Terminates the uv_loop.
     *
     * This is thread and async-signal safe. Can be called multiple
     * times. */
    void stop_async();
    bool is_stopped() const noexcept;
    bool is_stopped_or_stopping() const noexcept;
    void block_until_stopped() const;

    void remove_task_callbacks(size_t index);

    uv_loop_t *get_uv_loop();

    /* Note that in C++ derived classes are destructed before the parent
     * class. If any callbacks are defined in the parent class, and
     * those callbacks are called when libuv_wrapper is being detructed,
     * then you will have undefined behaviour because the parent class
     * and it's functions would have been destroyed.
     *
     * Where possible, you SHOULD call stop() in the top-most derived
     * class that defined any callbacks that could be called as libuv is
     * being shutdown.
     */
    virtual ~libuv_wrapper();

  private:
    uv_loop_t m_loop;

    mutable std::thread m_thread;                     /* mutable because of block_until_stopped() */
    std::unique_ptr<uv_async_t> m_async;              /* For stopping the loop */
    std::unique_ptr<uv_async_t> m_loop_started_async; /* For calling te on_loop_started callbacks */

    mutable std::mutex m_tasks_mutex;
    std::vector<cb_t> m_started_cbs;
    std::vector<cb_t> m_stopped_cbs;
    std::map<size_t, cb_t> libuv_setup_task_cbs;
    std::map<size_t, cb_wrapup_t> m_wrapup_tasks_cbs;

    size_t m_task_counter;

    mutable std::recursive_mutex m_start_stop_mutex;
    std::atomic<bool> m_uvloop_stop_requested;
    std::atomic<bool> m_uvloop_running =
        false; /* could use uv_loop_alive(&m_loop), but m_loop won't be initalised if loop has not
                  been started at least once*/

    /* Starts libuv loop, the callbacks can be nullptr */
    void start_libuv();

    size_t add_setup_task_cb(cb_t setup, cb_wrapup_t wrapup);
};

libuv_wrapper& get_global_uv_holder();

} // namespace sg
