#pragma once

#include "weak_function.h"
#include "sg/export/common.h"

#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <functional>

#include <uv.h>

#define THROW_ON_LIBUV_ERROR(fn)                                                                   \
do {                                                                                               \
        auto const err = (fn);                                                                     \
        if (err < 0)                                                                               \
            throw std::runtime_error(uv_strerror(err));                                            \
} while (0)

namespace sg::uv {

template <typename T>
struct deleter_uv_handle {
    void operator()(T* ptr) {
        /* if the loop is not running, just delete.
         *
         * uv_loop_alive is not reliable, so we also use ptr->loop->data */
        if (!uv_loop_alive(ptr->loop) || !ptr->loop->data)
        {
            delete ptr;
            ptr=nullptr;
            return;
        }

        /* the uv_loop is running, we have to:
         * - create an async_t object
         * - in the async object callback, request closure of the part
         * - in the callback fo the closure, delete this pointer and also the async pointer
         *   itself */

        // should not throw exceptions in deleters, if above malloc failes just quit
        uv_async_t* async_handle = (uv_async_t*)malloc(sizeof(uv_async_t));
        if (!(void*)async_handle)
            return;

        auto async_func = [](uv_async_t* async_handle) {
            auto hndl_to_close = (uv_handle_t*)async_handle->data;

            if(!uv_is_closing(hndl_to_close))
                uv_close(hndl_to_close, [](uv_handle_t* handle) {
                    delete handle;
                    handle=nullptr;
                });

            // remove the async handle itself
            uv_close((uv_handle_t*)async_handle,  [](uv_handle_t* h) {
                delete h;
                h=nullptr;
            });

        };


        if (uv_async_init(ptr->loop, async_handle, async_func)==0){
            // to the pointer to be deleted/closed in teh async handle
            async_handle->data = ptr;
            uv_async_send(async_handle);
        }
    };
};
template <typename T> using unqiue_uv_handle = std::unique_ptr<T, uv::deleter_uv_handle<T>>;

template <typename T>
unqiue_uv_handle<T> make_unique_uv_handle() {
    return  unqiue_uv_handle<T>(new T);
}

}

namespace sg {

/* wrapper around libuv, helps with starting and stopping it properly */
class SG_COMMON_EXPORT libuv_wrapper {
  public:
    enum class wrapup_result {
        rerun_uv_loop,
        stop_uv_loop
    };

    typedef std::function<void(libuv_wrapper *)> cb_t;
    typedef std::function<wrapup_result(libuv_wrapper *)> cb_wrapup_t;
    typedef size_t callback_id_t;

    /* add a callback to be called just before the uv_loop is started.
     *
     * Callback is called only once. You must add it if you need it to
     * be called on futher starts/stops. */
    callback_id_t add_on_loop_started_cb(cb_t);

    /* add a callback to be called just after the uv_loop is stopped.
     *
     * Callback is called only once. You must add it if you need it to
     * be called on further starts/stops. */
    callback_id_t add_on_stopped_cb(cb_t);

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
    callback_id_t start_task(cb_t setup_action, cb_wrapup_t wrapup_action);

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

    /* Note that in C++ child classes are destructed before the parent
     * class. If any callbacks are defined in the parent class, and
     * those callbacks are called when libuv_wrapper is being detructed,
     * then you will have undefined behaviour because the parent class
     * and it's functions would have been destroyed.
     *
     * Where possible, you SHOULD call stop() in the child class
     * that defined any callbacks that could be called as libuv is
     * being shutdown.
     */

    virtual ~libuv_wrapper();

  private:
    uv_loop_t m_loop;
    sg::uv::unqiue_uv_handle<uv_async_t> m_async;              /* For stopping the loop */
    sg::uv::unqiue_uv_handle<uv_async_t> m_loop_started_async; /* For calling te on_loop_started callbacks */

    mutable std::thread m_thread;                     /* mutable because of block_until_stopped() */

    mutable std::mutex m_tasks_mutex;
    std::map<callback_id_t, cb_t> m_started_cbs;
    std::map<callback_id_t, cb_t> m_stopped_cbs;
    std::map<size_t, cb_t> libuv_setup_task_cbs;
    std::map<size_t, cb_wrapup_t> m_wrapup_tasks_cbs;

    callback_id_t m_callback_counter{0};

    mutable std::recursive_mutex m_start_stop_mutex;
    std::atomic<bool> m_uvloop_stop_requested;
    std::atomic<bool> m_uvloop_running =
        false; /* could use uv_loop_alive(&m_loop), but m_loop won't be initalised if loop has not
                  been started at least once*/

    /* Starts libuv loop, the callbacks can be nullptr */
    void start_libuv();

    callback_id_t add_setup_task_cb(cb_t setup, cb_wrapup_t wrapup);

};

libuv_wrapper& get_global_uv_holder();

} // namespace sg
