#include "sg/hardware.h"

#include "sg/cpu.h"
#include "sg/file.h"
#include "sg/net.h"

#include <algorithm>
#include <filesystem>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winreg.h>
#endif

namespace {

[[nodiscard]] std::vector<std::string> get_disk_vendor() {
    std::vector<std::string> vendors;
#ifdef __linux
    for (const auto& blockDev : std::filesystem::directory_iterator("/sys/class/block")) {
        std::string str{};
        auto vendorPath = blockDev / std::filesystem::path("device/vendor");
        auto modelPath  = blockDev / std::filesystem::path("device/model");
        for (const auto& path : {vendorPath, modelPath})
            if (std::filesystem::exists(path))
                str += sg::common::file::get_contents(path) + " ";
        if (!str.empty())
            vendors.push_back(str);
    }
#elif defined(_WIN32)
    for (int i = 0;; i++) {
        DWORD bufferSize = 0;
        auto index       = std::to_string(i);
        LSTATUS result   = RegGetValueA(HKEY_LOCAL_MACHINE,
                                      "SYSTEM\\CurrentControlSet\\Services\\disk\\Enum",
                                      index.c_str(),
                                      RRF_RT_REG_SZ,
                                      nullptr,
                                      nullptr,
                                      &bufferSize);

        if (result == ERROR_MORE_DATA) {
            std::string buffer(bufferSize, '\0');
            result = RegGetValueA(HKEY_LOCAL_MACHINE,
                                  "SYSTEM\\CurrentControlSet\\Services\\disk\\Enum",
                                  index.c_str(),
                                  RRF_RT_REG_SZ,
                                  nullptr,
                                  buffer.data(),
                                  &bufferSize);
            if (result == ERROR_SUCCESS)
                vendors.push_back(buffer.data());
            else
                break;
        } else
            break;
    }
#endif

    return vendors;
}

}

namespace sg::common::hardware {

[[nodiscard]] SG_COMMON_EXPORT bool is_running_in_vm() {
    // CPU flag
    if (sg::cpu::is_hypervisor_flag_set())
        return true;

    // BIOS Name
    if (auto bios = bios_vendor();
        bios.find("VMware") != std::string::npos ||
        bios.find("VirtualBox") != std::string::npos ||
        bios.find("QEMU") != std::string::npos ||
        bios.find("Microsoft Corporation") != std::string::npos ||
        bios.find("Parallels") != std::string::npos)
        return true;

    // MAC address
    std::vector<std::array<char, 6>> macs;
    macs.push_back({0x00, 0x05, 0x69}); // VMWare
    macs.push_back({0x00, 0x50, 0x56}); // VMWare
    macs.push_back({0x00, 0x0C, 0x29}); // VMWare
    macs.push_back({0x00, 0x1C, 0x14}); // VMWare
    macs.push_back({0x08, 0x00, 0x27}); // VirtualBox
    macs.push_back({0x52, 0x54, 0x00}); // KVM
    macs.push_back({0x00, 0x15, 0x5D}); // Microsoft Hypervisor
    macs.push_back({0x00, 0x16, 0x3E}); // Microsoft Hypervisor
    for (const auto& interface : sg::net::interfaces())
        for (const auto& mac : macs)
            if (interface.physical_address[0] == mac[0] &&
                interface.physical_address[1] == mac[1] &&
                interface.physical_address[2] == mac[2])
                return true;

    // DIsk vendors
    std::vector<std::string> vedorsToCheck = {"qemu", "virtio", "vmware", "vbox", "xen", "virtual"};
    auto vendors = get_disk_vendor();
    for (const auto& vendor : vendors)
        for (auto toCheck : vedorsToCheck) {

            // Convert to lower case, works ok for ASCII characters
            std::transform(toCheck.begin(), toCheck.end(), toCheck.begin(), [](unsigned char c) {
                return std::tolower(c);
            });

            if (vendor.find(toCheck) != std::string::npos)
                return true;
        }

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