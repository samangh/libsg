#pragma once
#include <sg/export/common.h>

#include "callback.h"
#include "jthread.h"

#include <boost/asio/io_context.hpp>

#include <vector>

namespace sg::net {

/**
 * @brief A restartable thread pool driving a single Boost.Asio @c io_context.
 *
 * The pool owns a @c boost::asio::io_context and a fixed-size set of worker
 * threads, each of which calls @c io_context::run(). It can be started, stopped
 * and restarted any number of times during its lifetime, and an optional
 * user-supplied callback fires once per stop cycle.
 *
 * @par Lifecycle
 * Instances pass through three states (see @ref state_t):
 * @c stopped → @c running → @c stopping → @c stopped. Transitions are driven
 * by @ref run() and @ref stop_async(); the @c stopping state is transient.
 *
 * @par Thread safety
 * All public member functions are safe to call concurrently from multiple
 * threads. @ref run() and @ref stop_async() are mutually serialised via an
 * internal mutex.
 *
 * @par Ownership
 * Instances must be heap-allocated through @ref create(); as this class is intended to be shared
 * across multiple user classes (e.g. tcp_client / tcp_server).
 *
 * @par Stopped callback re-entrancy
 * The @c stopped_cb_t callback runs on a dedicated thread distinct from the
 * pool workers. From inside the callback:
 * - @ref stop_async() is a safe no-op.
 * - @ref wait_for_stop() returns immediately.
 * - @ref run() and @ref restart() throw @c std::logic_error.
 * The callback should not throw; an escaping exception terminates the program.
 *
 * @see boost::asio::io_context
 */
class SG_COMMON_EXPORT asio_io_pool {
    struct Private{ explicit Private() = default; };

  public:
    /**
     * @brief Lifecycle state of the pool.
     */
    enum class state_t {
        stopped, ///< Idle. No worker threads are running.
        running, ///< Workers are processing the @c io_context.
        stopping ///< Transient: workers are draining toward @c stopped.
    };

    /**
     * @brief Callback invoked once per stop cycle, after all worker threads
     *        have finished and the @c io_context has drained.
     *
     * The pool reference passed in refers to the instance that just stopped.
     * The callback runs on a dedicated thread; see the class-level
     * "Stopped callback re-entrancy" notes for the operations it may perform.
     */
    CREATE_CALLBACK(stopped_cb_t, void(asio_io_pool&))

    /**
     * @brief Tag-protected constructor. Use @ref create() instead.
     *
     * @param noWorkers          Number of worker threads. Must be > 0.
     * @param enableGuard        If @c true, the pool installs an
     *                           @c executor_work_guard that keeps the
     *                           @c io_context alive even when no work is
     *                           pending; only @ref stop_async() will then end
     *                           the run cycle.
     * @param onStoppedCallBack  Optional callback invoked once per stop cycle.
     *                           Maybe @c nullptr.
     */
    asio_io_pool(Private, size_t noWorkers, bool enableGuard, stopped_cb_t onStoppedCallBack);

    /**
     * @brief Stops the pool (if running) and waits for it to finish.
     *
     * Equivalent to calling @ref stop_async() followed by @ref wait_for_stop().
     */
    ~asio_io_pool();

    /**
     * @brief Starts the worker threads and begins running the @c io_context.
     *
     * This call does not clear handlers already queued on the @c io_context;
     * any work posted before @c run() is processed once the workers start.
     *
     * If the pool is currently in the @c stopping state, this call blocks until
     * the previous cycle has finished and then begins a new one.
     *
     * @return @c true if a new run cycle was started, @c false if the pool was
     *         already running.
     *
     * @throws std::logic_error if called from within the stopped-callback.
     * @throws std::bad_alloc, or any exception thrown by Boost.Asio during
     *         pool/context setup; in this case the pool is left in the
     *         @c stopped state.
     *
     * @note Even when @c true is returned, a guardless pool whose
     *       @c io_context has no work may immediately transition out of
     *       @c running before this function returns.
     */
    bool run();

    /**
     * @brief Stops the current run cycle and starts a new one.
     *
     * Equivalent to @ref stop_async() + @ref wait_for_stop() + @ref run().
     *
     * @throws std::logic_error if called from within the stopped-callback.
     */
    void restart();

    /**
     * @brief Requests the pool to stop and returns immediately.
     *
     * Drops the work guard (if any) and calls @c io_context::stop(). The
     * workers complete their currently-executing handlers and then exit.
     * Use @ref wait_for_stop() to block until the cycle completes.
     *
     * Safe to call when the pool is already @c stopping or @c stopped: in that
     * case it has no effect. Safe to call from a handler running on the pool
     * itself, and from the stopped-callback (where it is a no-op).
     */
    void stop_async();

    /**
     * @brief Blocks until the pool reaches the @c stopped state.
     *
     * If called from within the stopped-callback this returns immediately,
     * since the calling thread is the one that finalises the stop transition.
     *
     * @warning Without a prior @ref stop_async(), this can wait forever if the
     *          pool has a work guard (see @ref has_guard()).
     */
    void wait_for_stop() const;

    /**
     * @brief Returns whether the pool was constructed with a work guard.
     *
     * The flag is fixed at construction time; it does not reflect whether a
     * guard is currently installed (the guard is dropped while stopping).
     */
    [[nodiscard]] bool has_guard() const;

    /**
     * @brief Returns @c true if the pool is in the @c running state.
     */
    [[nodiscard]] bool is_running() const;

    /**
     * @brief Returns the number of worker threads (fixed at construction).
     */
    [[nodiscard]] size_t worker_count() const;

    /**
     * @brief Returns the current lifecycle state.
     *
     * The value is a snapshot; the state may change before the caller acts on
     * it.
     */
    [[nodiscard]] state_t state() const;

    /**
     * @brief Direct access to the underlying @c io_context.
     *
     * Use this to post handlers, create timers, sockets, etc. The context is
     * valid for the lifetime of the pool, across run cycles.
     */
    [[nodiscard]] boost::asio::io_context& context();

    /// @copydoc context()
    [[nodiscard]] const boost::asio::io_context& context() const;

    /**
     * @brief Constructs a new pool wrapped in a @c shared_ptr.
     *
     * @param noWorkers          Number of worker threads. If zero, defaults to
     *                           @c std::thread::hardware_concurrency().
     * @param enableGuard        See the constructor.
     * @param onStoppedCallBack  See the constructor.
     */
    [[nodiscard]] static std::shared_ptr<asio_io_pool>
    create(size_t noWorkers, bool enableGuard, stopped_cb_t onStoppedCallBack);

private:
    mutable std::mutex m_mutex;
    boost::asio::io_context m_context;

    const size_t m_no_workers;

    std::atomic<state_t> m_state{state_t::stopped};

    const bool m_guard_enabled;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_guard;

    const stopped_cb_t m_on_stopped_call_back;
    /// ID of @ref m_cb_thread while it is executing the user callback. Used by
    /// @ref run() and @ref wait_for_stop() to detect re-entry from the
    /// callback and avoid self-deadlock.
    std::atomic<std::thread::id> m_cb_thread_id{};

    /// Worker threads driving @ref m_context. Joined by @ref m_cb_thread, not
    /// by the pool itself.
    std::vector<std::jthread> m_workers;

    /// Monitor thread: waits for the cycle to leave @c state_t::running, joins
    /// the workers, runs the user callback, then publishes @c state_t::stopped.
    /// Must be the last member so that its @c ~jthread joins before any other
    /// member it touches is destroyed.
    std::jthread m_cb_thread;
};

}
