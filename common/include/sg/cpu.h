#pragma once

#include <sg/export/common.h>

#include <string>
#include <vector>

namespace sg::cpu {

enum class cpu_vendor { Amd, Intel, Arm, Other };

[[nodiscard]] SG_COMMON_EXPORT cpu_vendor current_cpu_vendor();

/** Estimate of the default amount of parallelism a program should use.
 *
 * Note, this is different in that it respects core isolation limitations (e.g. number of cpus
 * a container is limited to), whereas @c std::thread::hardware_concurrency() would just return the
 * CPU core/thread account, even if they all could not be used.
 */
[[nodiscard]] SG_COMMON_EXPORT size_t available_parallelism();

}
