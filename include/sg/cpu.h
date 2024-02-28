#pragma once

namespace sg::cpu {

enum class cpu_vendor {
    Amd,
    Intel,
    Other
};

cpu_vendor current_cpu_vendor();

}
