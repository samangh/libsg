#pragma once

#include <sg/export/common.h>

#include <string>
#include <vector>

namespace sg::cpu {

enum class cpu_vendor { Amd, Intel, Arm, Other };

struct cpu_info_t {
    std::string model;
    int speed;
};

[[nodiscard]] SG_COMMON_EXPORT cpu_vendor current_cpu_vendor();

/* Details of the current CPUs on the system.
 *
 * This function may be slow, don't call on a tight loop */
[[nodiscard]] SG_COMMON_EXPORT std::vector<cpu_info_t> info();

/** Estimate of the default amount of parallelism a program should use.
 *
 * Note, this is different in that it respects core isolation limitations (e.g. number of cpus
 * a container is limited to), whereas @c std::thread::hardware_concurrency() would just return the
 * CPU core/thread account, even if they all could not be used.
 */
[[nodiscard]] SG_COMMON_EXPORT size_t available_parallelism();

}
