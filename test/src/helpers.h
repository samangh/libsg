#pragma once

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <iostream>
#include <utility>

#include <sg/debug.h>

#ifdef _WIN32
    #include <process.h>
#else
    #include <unistd.h>
#endif

#ifdef __linux__
    #include <sys/prctl.h>
#endif

class scoped_deadline {
public:
    explicit scoped_deadline(std::string msg,
                             std::chrono::seconds limit = std::chrono::seconds(30))
    {
      m_thread = std::thread([this, msg, limit]() {
          const auto deadline = std::chrono::steady_clock::now() + limit;
          while (!m_done.load(std::memory_order::acquire)) {
              if (std::chrono::steady_clock::now() >= deadline) {
                  std::cerr << msg << std::endl;
                  dump_all_thread_stacks();
                  std::abort();
              }
              std::this_thread::sleep_for(std::chrono::milliseconds(20));
          }
      });
    }

    ~scoped_deadline() {
        m_done.store(true, std::memory_order::release);
        m_thread.join();
    }

    scoped_deadline(const scoped_deadline&)            = delete;
    scoped_deadline& operator=(const scoped_deadline&) = delete;

private:
    static bool on_path(const std::string& tool) {
#ifdef _WIN32
        return std::system(("where " + tool + " >NUL 2>&1").c_str()) == 0;
#else
        return std::system(("command -v " + tool + " >/dev/null 2>&1").c_str()) == 0;
#endif
    }

    /* A stalled test is only diagnosable from the stacks of the threads that are stuck, and an
     * in-process backtrace can only see this one -- so attach a debugger to ourselves instead. */
    static void dump_all_thread_stacks() {
#ifdef __linux__
        /* The debugger is our child, and yama only lets a *descendant* ptrace us by default. */
        prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
#endif

#ifdef _WIN32
        const auto pid = std::to_string(_getpid());
#else
        const auto pid = std::to_string(getpid());
#endif

        const std::pair<const char*, std::string> debuggers[] = {
            {"gdb", "gdb --batch -p " + pid + " -ex \"thread apply all bt\""},
            {"eu-stack", "eu-stack -p " + pid},
            {"lldb", "lldb -p " + pid + " --batch -o \"thread backtrace all\" -o detach"},
            {"cdb", "cdb -p " + pid + " -c \"~*k;q\""},
        };

        for (const auto& [tool, command] : debuggers) {
            if (!on_path(tool))
                continue;

            /* bounded twice over, so a debugger that can neither attach nor exit does not
             * replace one silent stall with another */
            const auto cmd = on_path("timeout") ? "timeout 120 " + command : command;
            std::thread([]() {
                std::this_thread::sleep_for(std::chrono::seconds(150));
                std::_Exit(134);
            }).detach();

            std::cerr << "scoped_deadline: dumping thread stacks with `" << tool << "`"
                      << std::endl;
            std::system(cmd.c_str());
            return;
        }

        std::cerr << "scoped_deadline: no debugger on PATH (gdb, eu-stack, lldb, cdb), so no "
                     "stacks; install one to diagnose this"
                  << std::endl;
    }

    std::atomic<bool> m_done{false};
    std::thread m_thread;
};

class redirect_sg_log_trace {
public:
    redirect_sg_log_trace() {
        m_stream = sg::debug::trace_stream();
        sg::debug::set_trace_stream(nullptr);
    }
    ~redirect_sg_log_trace() {
        sg::debug::set_trace_stream(m_stream);
    }
private:
    std::shared_ptr<std::ostream> m_stream;
};
