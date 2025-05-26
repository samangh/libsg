#include "sg/background_timer.h"

#include <chrono> // for operator-, high_resolution_c...
#include <mutex>         // for unique_lock, lock_guard, mutex
#include <stdexcept>     // for logic_error, runtime_error
#include <thread>

#include "sg/accurate_sleeper.h" // for AccurateSleeper, AccurateSle...
#include "sg/pimpl.h"            // for pimpl

/* Note:
 *
 * A thread that has finished executing code, but has not yet been joined is
 * still considered an active thread of execution and is therefore joinable.
 */

namespace sg {

class SG_COMMON_EXPORT background_timer::impl {
  public:
    /* encapsulates a thread that runs a provided task regualrly on a timer */
    impl(background_timer *timer_ref,
         const task_t &task,
         const started_cb_t &start_cb,
         const stopped_cb_t &stopped_cb);
    ~impl();

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
    void set_interval(uint32_t interval_ns, sg::AccurateSleeper::Tragedy strategy);

    /* sets whether the time shouuld account for how long the action takes,
     * and remove that from the wait interval */
    void correct_for_task_delay(bool);

  private:
    background_timer *m_timer_ref;

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
    bool m_correct_for_task_delay = false;

    mutable std::shared_mutex m_running_mutex;
    bool m_is_running = false;

    void action();
    void set_exception(std::exception_ptr ptr);
    void set_is_running(bool);
};

background_timer::impl::impl(background_timer *timer_ref,
                             const background_timer::task_t &task,
                             const background_timer::started_cb_t &start_cb,
                             const background_timer::stopped_cb_t &stopped_cb)
    : m_timer_ref(timer_ref),
      m_task(task),
      m_started_cb(start_cb),
      m_stopped_cb(stopped_cb) {}

background_timer::impl::~impl() {
    request_stop();
    wait_for_stop();
}

void background_timer::impl::start() {
    if (is_running())
        throw std::runtime_error("this worker is already running");

    /* if the thread has finished executing code,but has not been joined yet */
    wait_for_stop();

    /* we set these here, as usually there is a delay before a thread gets going
     * and so the user call call functions that use these variables before they are
     * set in the new thread */

    m_stop_requested = false;
    set_exception(nullptr);
    set_is_running(true);
    m_thread = std::thread(&background_timer::impl::action, this);
}

void background_timer::impl::wait_for_stop() {
    if (m_thread.get_id() != std::this_thread::get_id()) {
        /* called from different thread */
        if (m_thread.joinable())
            m_thread.join();
    } else {
        throw std::logic_error("can't wait for background_timer stop to stop from within itself");
    }
}

void background_timer::impl::action() {
    if (m_started_cb)
        m_started_cb(m_timer_ref);

    while (!is_stop_requested()) {
        std::shared_lock lock(m_sleeper_mutex);
        try {
            if (m_correct_for_task_delay) {
                auto t = std::chrono::high_resolution_clock::now();
                m_task(m_timer_ref);
                m_sleeper.sleep(std::chrono::high_resolution_clock::now() - t);
            } else {
                m_task(m_timer_ref);
                m_sleeper.sleep();
            }
        } catch (...) {
            set_exception(std::current_exception());
            break;
        }
    }

    set_is_running(false);
    if (m_stopped_cb)
        m_stopped_cb(m_timer_ref, get_exception());
}

void background_timer::impl::request_stop() {
    std::unique_lock lock(m_stop_mutex);
    m_stop_requested = true;
}

bool background_timer::impl::is_stop_requested() const {
    std::shared_lock lock(m_stop_mutex);
    return m_stop_requested;
}

bool background_timer::impl::has_exception() const {
    std::lock_guard lock(m_exception_mutex);
    return (bool)m_exception;
}

void background_timer::impl::set_exception(std::exception_ptr ptr) {
    std::lock_guard lock(m_exception_mutex);
    m_exception = ptr;
}

std::exception_ptr background_timer::impl::get_exception() const {
    std::lock_guard lock(m_exception_mutex);
    return m_exception;
}

void background_timer::impl::set_is_running(bool running) {
    std::unique_lock lock(m_running_mutex);
    m_is_running = running;
}

bool background_timer::impl::is_running() const {
    std::shared_lock lock(m_running_mutex);
    return m_is_running;
}

uint64_t background_timer::impl::interval() const {
    std::shared_lock lock(m_sleeper_mutex);
    return m_sleeper.interval();
}

void background_timer::impl::set_interval(uint32_t interval_ns,
                                          AccurateSleeper::Tragedy strategy) {
    std::unique_lock lock(m_sleeper_mutex);
    m_sleeper.set_interval(interval_ns, strategy);
}

void background_timer::impl::correct_for_task_delay(bool correct) {
    std::unique_lock lock(m_sleeper_mutex);
    m_correct_for_task_delay = correct;
}

background_timer::background_timer(const task_t &task,
                                   const started_cb_t &start_cb,
                                   const stopped_cb_t &stopped_cb)
    : pimpl(sg::pimpl<impl>(this, task, start_cb, stopped_cb)) {}

background_timer::~background_timer() = default;

void background_timer::start() { pimpl->start(); }
void background_timer::request_stop() { pimpl->request_stop(); }
void background_timer::wait_for_stop() { return pimpl->wait_for_stop(); }
bool background_timer::is_running() const { return pimpl->is_running(); }
bool background_timer::is_stop_requested() const { return pimpl->is_stop_requested(); }

std::exception_ptr background_timer::get_exception() const { return pimpl->get_exception(); }
bool background_timer::has_exception() const { return pimpl->has_exception(); }

uint64_t background_timer::interval() const { return pimpl->interval(); }
void background_timer::set_interval(uint32_t interval_ns, sg::AccurateSleeper::Tragedy strategy) {
    return pimpl->set_interval(interval_ns, strategy);
}

void background_timer::correct_for_task_delay(bool set) { pimpl->correct_for_task_delay(set); }

} // namespace sg
