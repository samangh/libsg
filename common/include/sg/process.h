#pragma once

#include <sg/export/common.h>

#include <filesystem>
#include <map>
#include <vector>

namespace sg::process {

#ifdef _WIN32
typedef unsigned int pid_t;
typedef unsigned int tid_t;
#elif (defined(__APPLE__) && defined(__MACH__))
typedef uint32_t pid_t;
typedef uint32_t tid_t;
#else
typedef int pid_t;
typedef int tid_t;
#endif

struct Thread {
    tid_t tid;
    bool is_kernel_thread;
    std::string name;
};

/* Process info
 *
 * Notes:
 *  - in Linux, the main thread in a process has same pid and tid
 *  - in Windows, the main thread in a process has different pid and tid
 *  - in macOS you can only get thread details for your own process, so the threads
 *    field is lef tempty.
 */
struct Process {
    pid_t pid;              // Process ID
    pid_t parent_pid;       // Parent process PID
    bool is_kernel_process; // Is a kernel thread
    size_t thread_count;
    std::string name;
    std::vector<std::string> cmdline;
    std::map<tid_t, Thread> threads;
};

SG_COMMON_EXPORT std::map<pid_t,Process> get_processes();

}
