#include "sg/environment.h"

#include "vmaware.hpp"

namespace sg::environment {

bool is_running_in_vm() {
   return VM::detect();
}

}
