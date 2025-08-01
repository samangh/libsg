#pragma once

#include "sg/export/common.h"

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

/* Estimate of the default amount of parallelism a program should use */
[[nodiscard]] SG_COMMON_EXPORT size_t available_parallelism();

/* returns true if teh hypervisor flag is set on the CPU */
[[nodiscard]] SG_COMMON_EXPORT bool is_hypervisor_flag_set();

}
