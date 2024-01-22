#pragma once

#include <functional>
#include <thread>
#include <stdexcept>
#include <uv.h>

#define THROW_ON_LIBUV_ERROR(err)                                                                  \
    do {                                                                                           \
        if (err < 0)                                                                               \
            throw std::runtime_error(uv_strerror(err));                                            \
    } while (0);

namespace sg {

/* wrapper around libuv, helps with starting and stopping it properly */
class libuv_wrapper {
  public:
    virtual ~libuv_wrapper();
    void stop();
    bool is_running() const;

    typedef std::function<void(libuv_wrapper *)> libuv_on_start_cb_t;
    typedef std::function<void(libuv_wrapper *)> libuv_on_stop_cb_t;

  protected:
    uv_loop_t m_loop;

    /* Function that will be called to setup libuv operations
     *
     * This function is called bye start_libuv() as part of the start process.
     * It'll be called after the libuv loopis initialised, but before the loop
     * itself is started.
     *
     * This function will be called in the same thread that the start_libuv()
     * function is called on. */
    virtual void setup_libuv_operations() = 0;

    /* Function to stop derived_class specific libuv operations
     *
     * Note:
     *  - This will be called right before uv_stop() is called.
     *  - This will be called in the libuv event loop thread. */
    virtual void stop_libuv_operations() = 0;

    /* Starts libuv loop, the callbacks can be nullptr */
    void start_libuv(libuv_on_start_cb_t, libuv_on_stop_cb_t);

  private:
    std::thread m_thread;
    std::unique_ptr<uv_async_t> m_async; /* For stopping the loop */

    libuv_on_start_cb_t m_started_cb;
    libuv_on_stop_cb_t m_stopped_cb;
};

} // namespace sg
