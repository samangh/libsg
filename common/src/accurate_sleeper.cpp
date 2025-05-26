#include "sg/accurate_sleeper.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <string>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    // clang-format off
    #include <windows.h>
    #include <timeapi.h>
// clang-format on
#else
    #include <errno.h>
    #include <pthread.h>
    #include <time.h>
#endif

#ifdef __linux__
    #include <sched.h>
#endif

sg::AccurateSleeper::AccurateSleeper() {}
sg::AccurateSleeper::~AccurateSleeper() { disable_realtime(); }

void sg::AccurateSleeper::set_interval(uint64_t interval_ns, Tragedy strategy) {
    if (strategy == Tragedy::Auto) {
        /* Use tight loop if period is 1 ms or less */
        if (interval_ns > 1000000)
            m_strategy = Tragedy::Sleep;
        else
            m_strategy = Tragedy::Spin;
    } else
        m_strategy = strategy;

    if (m_strategy == Tragedy::Sleep)
        disable_realtime();
    else
        enable_realtime();

    m_interval_ns = interval_ns;
}

uint64_t sg::AccurateSleeper::interval() const { return m_interval_ns; }

void sg::AccurateSleeper::enable_realtime() {
    if (!m_realtime_enabled) {
#if defined(__linux__) || defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
        /* Store the thread, so tha later on we restore the schedulign policy for teh same thread */
        m_thread = pthread_self();

        /* Get current policy for later restoration */
        m_previous_policy = std::make_unique<int>();
        m_previous_param = std::make_unique<struct sched_param>();
        if (pthread_getschedparam(m_thread, m_previous_policy.get(), m_previous_param.get()) != 0)
            throw std::runtime_error(
                std::string("Unable to get the current thread scheduling parameters, ") +
                strerror(errno));

        /* Set realtime priority to minimum+1 */
        struct sched_param param;
    #ifdef __linux__
        param.sched_priority = sched_get_priority_min(SCHED_FIFO) + 1;
    #else
        param.sched_priority = 2;
    #endif

        /* Set the scheduler using POSIX thread */
        int result = pthread_setschedparam(m_thread, SCHED_FIFO, &param);
        if (result != 0) {
            if (result == EPERM)
                std::cerr
                    << "Unable to enable real-time scheduling due to inadequate premissions. If in "
                       "Linux, use \"sudo setcap 'cap_sys_nice=eip' <program>\" to fix this."
                    << std::endl;
            throw std::runtime_error(std::string("Unable to enable real-time scheduling, ") +
                                     strerror(result));
        }
#elif defined(_WIN32)
        timeBeginPeriod(1);
#endif

        m_realtime_enabled = true;
    }
}

void sg::AccurateSleeper::disable_realtime() {
    if (m_realtime_enabled) {
#if defined(__linux__) || defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
        if (pthread_setschedparam(m_thread, *(m_previous_policy.get()), m_previous_param.get()) !=
            0)
            if (errno !=
                ESRCH) // Ignore ESRCH error, as that means that the thread was already finished
                throw std::runtime_error(std::string("Unable to disable real-time scheduling, ") +
                                         strerror(errno));
#elif defined(_WIN32)
        timeEndPeriod(1);
#endif
        m_realtime_enabled = false;
    }
}

void sg::AccurateSleeper::sleep() { sleep_remove_lag(0); }

void sg::AccurateSleeper::sleep_remove_lag(uint64_t remove_ns) {
    if (remove_ns > m_interval_ns)
        return;

    uint64_t interval = m_interval_ns - remove_ns;

    /* Tight loop */
    if (m_strategy == Tragedy::Spin) {
        auto s = std::chrono::high_resolution_clock::now();
        while ((std::chrono::high_resolution_clock::now() - s) < std::chrono::nanoseconds(interval))
            ;
        return;
    }

    /* Wait mode */
#if defined(__linux__) || defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
    struct timespec ts;
    ts.tv_sec = interval / 1E9;
    ts.tv_nsec = interval % 1000000000;

    /* Call until the time is passed, see nanosleep() manual) */
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
        ;
        // pselect(0, NULL, NULL, NULL, &ts, NULL);
#elif defined(_WIN32)
    auto milliseconds = (DWORD)m_interval_ns / 1000000;
    Sleep(milliseconds);
#endif
}
