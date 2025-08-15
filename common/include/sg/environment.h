#pragma once

#include "sg/export/common.h"
#define __STDC_LIMIT_MACROS
#include <cstdint>

#if INTPTR_MAX == INT32_MAX
    #define ENV_32BIT
#elif INTPTR_MAX == INT64_MAX
    #define ENV_64BIT
#endif

namespace sg::environment {

/** Returns true if running under a VM or container. Can be an expensive operation,
 *  best cache the result.
 */
[[nodiscard]] SG_COMMON_EXPORT bool is_running_in_vm();

}
