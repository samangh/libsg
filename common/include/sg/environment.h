#pragma once

#define __STDC_LIMIT_MACROS
#include <cstdint>

#if INTPTR_MAX == INT32_MAX
    #define ENV_32BIT
#elif INTPTR_MAX == INT64_MAX
    #define ENV_64BIT
#endif
