#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>

#ifndef _WIN32
    #include <pthread.h>
#endif

namespace udaq::common {

class AccurateSleeper {
  public:
    /* Defines the wait strategy for AccurateSleepr */
    enum class Strategy {
        Auto,  // Let system choose based on the internval
        Sleep, // Set the schedulign policy to real time and use sleep
        Spin   // Sping on a tight loop
    };

    AccurateSleeper();
    ~AccurateSleeper();
    template <class _reprsenetation, class _value>
    void set_interval(
        const std::chrono::duration<_reprsenetation, _value> &duration, Strategy strategy) {
        if (duration > std::chrono::nanoseconds::max())
            throw std::invalid_argument("The provided time internval is too long");

        set_interval(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count(), strategy);
    }
    void set_interval(uint32_t interval_ns, Strategy strategy);
    void enable_realtime();
    void disable_realtime();
    void sleep();

  private:
    uint64_t m_interval_ns = 1E9; //default: 1 second
    std::atomic<bool> m_realtime_enabled = false;
    Strategy m_strategy;

#ifndef _WIN32
    pthread_t m_thread;
    std::unique_ptr<int> m_previous_policy;
    std::unique_ptr<struct sched_param> m_previous_param;
#endif
};

} // namespace udaq::common
