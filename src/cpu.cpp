#include <sg/cpu.h>

#include <string>
#include <cstdint>

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

}

namespace sg::cpu {

cpu_vendor current_cpu_vendor() {
    static bool alreadyCalled = false;
    static cpu_vendor result = cpu_vendor::Other;

    if (alreadyCalled)
        return result;
    alreadyCalled=true;

    // when cpuid is called with parameter 0, it gives the CPU vendor string
    CPUID cpuID(0);
    std::string vendor;
    vendor += std::string((const char *)&cpuID.EBX(), 4);
    vendor += std::string((const char *)&cpuID.EDX(), 4);
    vendor += std::string((const char *)&cpuID.ECX(), 4);

    if (vendor == "AuthenticAMD")
        result = cpu_vendor::Amd;
    if (vendor == "GenuineIntel")
        result = cpu_vendor::Intel;

    return result;
}

}
