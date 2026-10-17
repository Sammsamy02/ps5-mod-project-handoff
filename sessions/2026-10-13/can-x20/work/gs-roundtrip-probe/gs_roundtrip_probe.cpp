// No-game PS5 GS round trip. Build-only until console access and test-owner
// authorization return. The probe never loads a PE or touches game memory.
#if !defined(__PROSPERO__)
#error "This diagnostic is only for the PS5 payload SDK target"
#endif

#include "verified_gs_segment_switch.h"

#include <machine/sysarch.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

int main()
{
    alignas(16) uint8_t guest_teb[0x70]{};
    void* guest_tls_slot[1]{};
    const uint64_t self = reinterpret_cast<uint64_t>(guest_teb);
    const uint64_t tls = reinterpret_cast<uint64_t>(guest_tls_slot);
    std::memcpy(guest_teb + 0x30, &self, sizeof(self));
    std::memcpy(guest_teb + 0x58, &tls, sizeof(tls));

    void* host_before = nullptr;
    if (sysarch(AMD64_GET_GSBASE, &host_before) != 0)
    {
        std::puts("[GS][PROBE] verdict=FAIL step=read-host-before");
        return 2;
    }
    std::printf("[GS][PROBE] host-before=%p\n", host_before);
    std::fflush(stdout);

    cap13::VerifiedGsSegmentSwitch switcher(cap13::ps5_sysarch_gs_operations());
    if (!switcher.enter(guest_teb))
    {
        std::puts("[GS][PROBE] verdict=FAIL step=enter-or-readback");
        return 3;
    }

    uint64_t observed_self = 0;
    uint64_t observed_tls = 0;
    asm volatile("movq %%gs:0x30, %0" : "=r"(observed_self) : : "memory");
    asm volatile("movq %%gs:0x58, %0" : "=r"(observed_tls) : : "memory");
    switcher.leave();

    void* host_after = nullptr;
    if (sysarch(AMD64_GET_GSBASE, &host_after) != 0)
    {
        std::puts("[GS][PROBE] verdict=FAIL step=read-host-after");
        return 4;
    }
    const bool self_ok = observed_self == self;
    const bool tls_ok = observed_tls == tls;
    const bool restored = host_after == host_before;
    std::printf("[GS][PROBE] self=%s tls=%s host-restored=%s\n",
                self_ok ? "PASS" : "FAIL", tls_ok ? "PASS" : "FAIL",
                restored ? "PASS" : "FAIL");
    std::printf("[GS][PROBE] verdict=%s\n",
                self_ok && tls_ok && restored ? "PASS" : "FAIL");
    return self_ok && tls_ok && restored ? 0 : 5;
}
