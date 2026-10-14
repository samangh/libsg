#pragma once

#include "worker.h"

namespace sg {

[[deprecated("this class has been renamed to 'worker'")]]
typedef worker notifiable_background_worker;
}