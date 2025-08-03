#include <mutex>
#include <sg/cpu.h>

#include <uv.h>

#include <string>
#include <cstdint>
#include <stdexcept>

#ifdef _WIN32
    #include <intrin.h>
#endif

#if defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64) ||            \
    defined(__ARM_ARCH)
    #define IS_ARM
#endif

namespace {

#ifndef IS_ARM
// From: https://stackoverflow.com/questions/1666093/cpuid-implementations-in-c
// Note: does not work in Arm, maybe we should use libuv cpuinfo instead?
class CPUID {
    uint32_t regs[4];

  public:
    explicit CPUID(unsigned i) {
    #ifdef _WIN32
        __cpuid((int*)regs, (int)i);

    #else
        asm volatile("cpuid"
                     : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
                     : "a"(i), "c"(0));
            // ECX is set to zero for CPUID function 4
    #endif
    }

    [[nodiscard]] const uint32_t& EAX() const { return regs[0]; }
    [[nodiscard]] const uint32_t& EBX() const { return regs[1]; }
    [[nodiscard]] const uint32_t& ECX() const { return regs[2]; }
    [[nodiscard]] const uint32_t& EDX() const { return regs[3]; }
};
#endif

struct internal_cpu_vendor {
    internal_cpu_vendor() {
        result = sg::cpu::cpu_vendor::Other;

#ifdef IS_ARM
        result = sg::cpu::cpu_vendor::Arm;
#else
        CPUID       cpuID(0);
        std::string vendor;
        vendor += std::string((const char*)&cpuID.EBX(), 4);
        vendor += std::string((const char*)&cpuID.EDX(), 4);
        vendor += std::string((const char*)&cpuID.ECX(), 4);

        if (vendor == "AuthenticAMD")
            result = sg::cpu::cpu_vendor::Amd;
        if (vendor == "GenuineIntel")
            result = sg::cpu::cpu_vendor::Intel;
#endif
    }

    sg::cpu::cpu_vendor result;
};
}

namespace sg::cpu {

cpu_vendor current_cpu_vendor() {
    static internal_cpu_vendor singleton;
    return singleton.result;
}

size_t available_parallelism() {
    static size_t count = uv_available_parallelism();
    return count;
}

bool is_hypervisor_flag_set() {
#ifdef IS_ARM
    return false;
#else
    const CPUID cpuID(0);
    return (cpuID.ECX() & (1 << 31)) != 0;
#endif
}

std::vector<cpu_info_t> info()
{
    uv_cpu_info_t *uv_info=nullptr;
    int count;

    int err = uv_cpu_info(&uv_info, &count);
    if (err <0)
        throw std::runtime_error(uv_strerror(err));

    std::vector<sg::cpu::cpu_info_t> m_cpu_infos;
    for (int i= 0; i < count; ++i)
    {
        sg::cpu::cpu_info_t cpu;
        cpu.model = uv_info[i].model;
        cpu.speed = uv_info[i].speed;
        m_cpu_infos.push_back(cpu);
    }

    uv_free_cpu_info(uv_info, count);

    return m_cpu_infos;
}

}
