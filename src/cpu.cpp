#include <mutex>
#include <sg/cpu.h>

#include <string>
#include <cstdint>

#include <uv.h>

#ifdef _WIN32
    #include <intrin.h>
#endif

namespace {

// From: https://stackoverflow.com/questions/1666093/cpuid-implementations-in-c
class CPUID {
    uint32_t regs[4];

  public:
    explicit CPUID(unsigned i) {
#ifdef _WIN32
        __cpuid((int *)regs, (int)i);

#else
        asm volatile
            ("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3])
             : "a" (i), "c" (0));
        // ECX is set to zero for CPUID function 4
#endif
    }

    const uint32_t &EAX() const {return regs[0];}
    const uint32_t &EBX() const {return regs[1];}
    const uint32_t &ECX() const {return regs[2];}
    const uint32_t &EDX() const {return regs[3];}
};

struct internal_cpu_vendor {
    internal_cpu_vendor() {
        CPUID cpuID(0);
        std::string vendor;
        vendor += std::string((const char *)&cpuID.EBX(), 4);
        vendor += std::string((const char *)&cpuID.EDX(), 4);
        vendor += std::string((const char *)&cpuID.ECX(), 4);

        if (vendor == "AuthenticAMD")
            result = sg::cpu::cpu_vendor::Amd;
        if (vendor == "GenuineIntel")
            result = sg::cpu::cpu_vendor::Intel;
    }
    sg::cpu::cpu_vendor result;
};

}

namespace sg::cpu {

cpu_vendor current_cpu_vendor() {
    static internal_cpu_vendor singleton;
    return singleton.result;
}

size_t available_parallelism()
{
    static size_t count = uv_available_parallelism();
    return count;
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
