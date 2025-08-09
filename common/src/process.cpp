#include "sg/process.h"

#include <filesystem>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <tlhelp32.h>
#elif defined(__linux)
    #include <pfs/procfs.hpp>
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
            Process process;
            process.pid        = pe32.th32ProcessID;
            process.parent_pid = pe32.th32ParentProcessID;
            process.name       = pe32.szExeFile;
            result.emplace(pe32.th32ProcessID, std::move(process));
        } while (Process32Next(hProcessSnap, &pe32));

        /* loop through all threads */
        if (!Thread32First(hProcessSnap, &te32))
            throw std::runtime_error("CFailed first thread");

        do {
            auto& process = result.at(te32.th32OwnerProcessID);

            Thread thread;
            thread.tid  = te32.th32ThreadID;
            thread.name = process.name;
            process.threads.emplace(te32.th32ThreadID, std::move(thread));
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
            .name              = processStatus.name,
            .cmdline = std::vector<std::filesystem::path>(processCmds.begin(), processCmds.end()),
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
    #else
    throw std::runtime_error("Not implemented");
    #endif

    return result;
}
} // namespace sg::process