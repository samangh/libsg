#pragma once

#include "sg/export/sg_common.h"

#include <string>
#include <vector>

namespace sg::cpu {

enum class cpu_vendor {
    Amd,
    Intel,
	Arm,
    Other
};

struct cpu_info_t {
    std::string model;
    int speed;
};

SG_COMMON_EXPORT cpu_vendor current_cpu_vendor();

/* Details of the current CPUs on the system.
 *
 * This function may be slow, don't call on a tight loop */
SG_COMMON_EXPORT std::vector<cpu_info_t> info();

/* Estimate of the default amount of parallelism a program should use */
SG_COMMON_EXPORT size_t available_parallelism();

}
