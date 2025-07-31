#include "sg/hardware.h"

#include "sg/cpu.h"
#include "sg/file.h"

#include <filesystem>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #inlude <winreg.h>
#endif

namespace sg::common::hardware {

[[nodiscard]] SG_COMMON_EXPORT bool is_running_in_vm() {
    if (sg::cpu::is_hypervisor_flag_set())
        return true;

    auto bios = bios_vendor();
    if (bios.find("VMware") != std::string::npos || bios.find("VirtualBox") != std::string::npos ||
        bios.find("QEMU") != std::string::npos ||
        bios.find("Microsoft Corporation") != std::string::npos ||
        bios.find("Parallels") != std::string::npos)
        return true;

    return false;
}

std::string bios_vendor() {
#ifdef __linux
    if (std::filesystem::exists("/sys/class/dmi/id/bios_vendor"))
        return sg::common::file::get_contents("/sys/class/dmi/id/bios_vendor");
#elif defined(_WIN32)
    char buffer[256];
    DWORD bufferSize = sizeof(buffer);
    LSTATUS result   = RegGetValueA(HKEY_LOCAL_MACHINE,
                                  "HARDWARE\\DESCRIPTION\\System\\BIOS",
                                  "SystemManufacturer",
                                  RRF_RT_REG_SZ,
                                  nullptr,
                                  buffer,
                                  &bufferSize);

    if (result == ERROR_SUCCESS)
        return std::string(buffer);
#endif
    return "";
}

} // namespace sg::common::environment