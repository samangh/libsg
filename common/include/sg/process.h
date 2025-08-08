#pragma once

#include <sg/export/common.h>

#include <filesystem>
#include <map>
#include <vector>

namespace sg::process {

#ifdef _WIN32
typedef unsigned int pid_t;
typedef unsigned int tid_t;
#else
typedef int pid_t;
typedef int tid_t;
#endif

struct Thread {
    tid_t tid;               // Process/thread ID
    bool is_kernel_thread; // Is a kernel thread
    std::string name;
};

struct Process {
    pid_t pid;               // Process/thread ID
    pid_t parent_pid;        // Parent process PID
    bool is_kernel_process; // Is a kernel thread
    std::string name;
    std::vector<std::filesystem::path> cmdline;
    std::map<tid_t,Thread> threads;
};

SG_COMMON_EXPORT std::map<pid_t,Process> get_processes();

}