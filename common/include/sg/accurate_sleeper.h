#pragma once

#include <sg/export/common.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>

#ifndef _WIN32
    #include <pthread.h>
#endif

namespace sg {

class SG_COMMON_EXPORT AccurateSleeper {
  public:
    /* Defines the wait strategy for AccurateSleepr */
    enum class Tragedy {
        Auto,  // Let system choose based on the internval
        Sleep, // Set the schedulign policy to real time and use sleep
        Spin   // Sping on a tight loop
    };

    AccurateSleeper();
    ~AccurateSleeper();
    template <class _reprsenetation, class _value>
    void set_interval(const std::chrono::duration<_reprsenetation, _value>& duration,
                      Tragedy strategy);
    void set_interval(uint64_t interval_ns, Tragedy strategy);
    uint64_t interval() const;
    void enable_realtime();
    void disable_realtime();

    void sleep();
    /* sleep for the usual interval, shorted by the given value. Intented for
     * use by background_time or other process which use accurate_sleeper as a timer.*/
    void sleep_remove_lag(uint64_t remove_from_interval_ns);

    template <class _reprsenetation, class _value>
    void sleep(const std::chrono::duration<_reprsenetation, _value>& duration);

  private:
    uint64_t m_interval_ns = 1000000000ULL; // default: 1 second
    std::atomic<bool> m_realtime_enabled = false;
    Tragedy m_strategy;

#ifndef _WIN32
    pthread_t m_thread;
    std::unique_ptr<int> m_previous_policy;
    std::unique_ptr<struct sched_param> m_previous_param;
#endif
};

template <class _reprsenetation, class _value>
void AccurateSleeper::set_interval(const std::chrono::duration<_reprsenetation, _value>& duration,
                                   Tragedy strategy) {
    if (duration > (std::chrono::nanoseconds::max)())
        throw std::invalid_argument("The provided time internal is too long");

    set_interval(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count(), strategy);
}

template <class _reprsenetation, class _value>
void AccurateSleeper::sleep(const std::chrono::duration<_reprsenetation, _value>& duration) {
    if (duration > (std::chrono::nanoseconds::max)())
        throw std::invalid_argument("the provided time interval is too long");
    sleep_remove_lag(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
}

} // namespace sg
