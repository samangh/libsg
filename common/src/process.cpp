#include "sg/process.h"

#include <filesystem>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <tlhelp32.h>
#elif defined(__linux)
    #include <pfs/procfs.hpp>
#elif (defined(__APPLE__) && defined(__MACH__))
    #include <libproc.h>
#endif

namespace sg::process {

std::map<pid_t,Process> get_processes() {
    std::map<pid_t,Process> result;

    #if defined(_WIN32)
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    THREADENTRY32 te32;

    try {
        hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);
        if (hProcessSnap == INVALID_HANDLE_VALUE)
            throw std::runtime_error("CreateToolhelp32Snapshot (of processes) failed");

        pe32.dwSize = sizeof(PROCESSENTRY32);
        te32.dwSize = sizeof(THREADENTRY32);

        /* loop through all processes */
        if (!Process32First(hProcessSnap, &pe32))
            throw std::runtime_error("Failed getting first process");
        do {
            result.emplace(
                pe32.th32ProcessID,
                Process{.pid               = pe32.th32ProcessID,
                        .parent_pid        = pe32.th32ParentProcessID,
                        .is_kernel_process = (std::string(pe32.szExeFile) == "System" ||
                                              std::string(pe32.szExeFile) == "[System Process]"),
                        .thread_count      = static_cast<size_t>(pe32.cntThreads),
                        .name              = pe32.szExeFile,
                        .cmdline           = {pe32.szExeFile}});
        } while (Process32Next(hProcessSnap, &pe32));

        /* loop through all threads */
        if (!Thread32First(hProcessSnap, &te32))
            throw std::runtime_error("CFailed first thread");

        do {
            auto& process = result.at(te32.th32OwnerProcessID);
            process.threads.emplace(te32.th32ThreadID,
                                    Thread{.tid              = te32.th32ThreadID,
                                           .is_kernel_thread = process.is_kernel_process,
                                           .name             = process.name});
        } while (Thread32Next(hProcessSnap, &te32));

        CloseHandle(hProcessSnap);
    } catch (...) {
        CloseHandle(hProcessSnap);
        throw;
    }
    #elif defined(__linux)
    for (const auto& p : pfs::procfs().get_processes()) {
        const auto processStatus = p.get_status();
        const auto processCmds   = p.get_cmdline();

        Process process{
            .pid               = p.id(),
            .parent_pid        = processStatus.ppid,
            .is_kernel_process = pfs::task::is_kernel_thread(p.get_stat()),
            .thread_count      = processStatus.threads,
            .name              = processStatus.name,
            .cmdline = std::vector<std::string>(processCmds.begin(), processCmds.end()),
            .threads = {}
        };

        for (const auto& t : p.get_tasks()) {
            const auto threadStatus = t.get_status();
            process.threads.emplace(
                t.id(),
                Thread{.tid              = t.id(),
                       .is_kernel_thread = pfs::task::is_kernel_thread(t.get_stat()),
                       .name             = threadStatus.name});
        }

        result.emplace(p.id(), std::move(process));
    }
    #elif (defined(__APPLE__) && defined(__MACH__))

    // Get size of buffer
    int bufferSize = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);

    //Create and populate buffer
    int* processBuffer = (int*)malloc(sizeof(int) * bufferSize);
    int k = proc_listpids(PROC_ALL_PIDS, 0, processBuffer, bufferSize * sizeof(int));

    for (int i = 0; i < k; i++) {
        int pid = processBuffer[i];

        struct proc_taskallinfo bsdInfo;
        proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &bsdInfo, sizeof(bsdInfo));

        char path[PROC_PIDPATHINFO_MAXSIZE];
        proc_pidpath(pid, path, sizeof(path));

        result.emplace(pid,
                       Process{.pid               = bsdInfo.pbsd.pbi_pid,
                               .parent_pid        = bsdInfo.pbsd.pbi_ppid,
                               .is_kernel_process = (pid == 0),
                               .thread_count = static_cast<size_t>(bsdInfo.ptinfo.pti_threadnum),
                               .name         = bsdInfo.pbsd.pbi_name,
                               .cmdline      = {path}});
    }

    #else
    #error sg::process::get_processes() not implemented for this operating system
    #endif

    return result;
}
} // namespace sg::process
