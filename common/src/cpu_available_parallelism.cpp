/* **************************************************************************
 * Port of libuv's uv_available_parallelism(), which is under an MIT license.
 * See LICENCES/libuv.md
 * **************************************************************************
 *
 * Copyright (c) 2015-present libuv project contributors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* The original implementation lives in src/unix/core.c, src/unix/linux.c and
 * src/win/util.c in the libuv sources. Only Windows, Linux and macOS are
 * properly supported here, everything else gets a best guess. */

#include <sg/cpu.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <bit>
#elif defined(__linux__)
    #include <algorithm>
    #include <climits>
    #include <cstring>
    #include <fstream>
    #include <iterator>
    #include <optional>
    #include <sched.h>
    #include <sstream>
    #include <string>
    #include <unistd.h>
#elif (defined(__APPLE__) && defined(__MACH__))
    #include <sys/sysctl.h>
    #include <unistd.h>
#else
    #include <thread>
#endif

namespace {

#if defined(__linux__)

std::optional<std::string> read_file(const std::string& path) {
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    if (!stream.is_open())
        return std::nullopt;

    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::optional<long long> read_number(const std::string& path) {
    auto contents = read_file(path);
    if (!contents)
        return std::nullopt;

    long long value;
    std::istringstream stream(*contents);
    if (!(stream >> value))
        return std::nullopt;

    return value;
}

/* The cgroup v2 entry of /proc/self/cgroup is always of the form "0::<path>",
 * see https://docs.kernel.org/admin-guide/cgroup-v2.html */
std::optional<long long> cgroup_v2_cpu_quota(const std::string& cgroup) {
    static const std::string mount_point = "/sys/fs/cgroup";

    auto end  = cgroup.find('\n', 4);
    auto path = mount_point + "/" + cgroup.substr(4, end - 4);

    /* If there are no controllers this isn't really a cgroup. */
    if (!read_file(path + "/cgroup.controllers"))
        return std::nullopt;

    /* Limits apply hierarchically, so walk up the tree from the current cgroup to
     * the mount point and use the most restrictive quota found on the way. */
    std::optional<long long> quota;
    while (path.starts_with(mount_point)) {
        if (auto contents = read_file(path + "/cpu.max")) {
            long long limit;
            long long period;
            std::istringstream stream(*contents);

            /* A limit of "max" means unconstrained and fails to parse, as intended. */
            if ((stream >> limit >> period) && period != 0)
                quota = std::min(quota.value_or(LLONG_MAX), std::max(1LL, limit / period));
        }

        auto last_slash = path.rfind('/');
        if (path == mount_point || last_slash == std::string::npos)
            break;
        path.resize(last_slash);
    }

    return quota;
}

std::optional<long long> cgroup_v1_cpu_quota(const std::string& cgroup) {
    auto controller = cgroup.find(":cpu,");
    if (controller == std::string::npos)
        return std::nullopt;

    auto start = controller + std::string(":cpu,").size();
    auto end   = cgroup.find('\n', start);
    auto path  = "/sys/fs/cgroup/" + cgroup.substr(start, end - start);

    auto quota_per_period = read_number(path + "/cpu.cfs_quota_us");
    auto period_length    = read_number(path + "/cpu.cfs_period_us");

    if (!quota_per_period || !period_length || *period_length == 0)
        return std::nullopt;

    return *quota_per_period / *period_length;
}

std::optional<long long> constrained_cpu() {
    auto cgroup = read_file("/proc/self/cgroup");
    if (!cgroup)
        return std::nullopt;

    if (cgroup->starts_with("0::/"))
        return cgroup_v2_cpu_quota(*cgroup);

    return cgroup_v1_cpu_quota(*cgroup);
}

#endif

} // namespace

namespace sg::cpu {

size_t available_parallelism() {
#if defined(_WIN32)
    DWORD_PTR process_mask = 0;
    DWORD_PTR system_mask  = 0;

    /* Note: like libuv, this only sees the processor group the process is bound
     * to, so it tops out at 64 CPUs. GetLogicalProcessorInformationEx() would be
     * needed to do better. */
    if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask)) {
        auto count = std::popcount(process_mask);
        if (count > 0)
            return static_cast<size_t>(count);
    }

    return 1;
#elif defined(__linux__)
    long count    = -1;
    cpu_set_t set = {};

    /* try sched_getaffinity() before falling back to sysconf(_SC_NPROCESSORS_ONLN). */
    if (sched_getaffinity(0, sizeof(set), &set) == 0)
        count = CPU_COUNT(&set);

    if (count < 0)
        count = sysconf(_SC_NPROCESSORS_ONLN);

    auto quota = constrained_cpu();
    if (quota && *quota > 0 && *quota < count)
        count = static_cast<long>(*quota);

    if (count < 1)
        count = 1;

    return static_cast<size_t>(count);
#elif (defined(__APPLE__) && defined(__MACH__))
    static const char* mibs[] = {"hw.activecpu", "hw.logicalcpu", "hw.ncpu"};

    for (const auto& mib : mibs) {
        int nprocs = 0;
        size_t len = sizeof(nprocs);

        if (sysctlbyname(mib, &nprocs, &len, nullptr, 0) == 0 && len == sizeof(nprocs) &&
            nprocs > 0)
            return static_cast<size_t>(nprocs);
    }

    auto count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count < 1)
        count = 1;

    return static_cast<size_t>(count);
#else
    auto count = std::thread::hardware_concurrency();
    return count < 1 ? 1 : static_cast<size_t>(count);
#endif
}

} // namespace sg::thread
