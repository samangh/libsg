#pragma once

#include <version>

#ifndef  __cpp_lib_jthread
    #include "internal/jthread/jthread.hpp"
#else
    #include <thread>
#endif
