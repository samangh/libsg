#pragma once

#include <string>
#include <vector>

namespace sg::cpu {

enum class cpu_vendor {
    Amd,
    Intel,
    Other
};

struct cpu_info_t {
    std::string model;
    int speed;
};

cpu_vendor current_cpu_vendor();

/* Details of the current CPUs on the system.
 *
 * This function may be slow, don't call on a tight loop */
std::vector<cpu_info_t> info();

/* Estimate of the default amount of parallelism a program should use */
size_t available_parallelism();

}
