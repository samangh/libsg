#include "sg/process.h"

#include <filesystem>
#include <pfs/procfs.hpp>

namespace sg::process {

std::map<pid_t,Process> get_processes() {
    std::map<pid_t,Process> result;

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

    return result;
}
} // namespace sg::process