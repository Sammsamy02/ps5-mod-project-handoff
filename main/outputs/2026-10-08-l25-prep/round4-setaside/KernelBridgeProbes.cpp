// [KB] Kernel-bridge observe-only probes — implementation. See the header
// for the ABI grounding and the scope law. Whole TU is double-gated:
// __PROSPERO__ AND MODBRIDGE_KB_PROBES (default OFF — zero code shipped).
#include "KernelBridgeProbes.hpp"

#if defined(__PROSPERO__) && defined(MODBRIDGE_KB_PROBES)

#include <cstdio>
#include <sys/types.h>
#include <unistd.h>

// sysctl is the R0 value source (own pid, plain call, no kekcall needed
// for the value itself — the kekcall is only exercised by the CHECK gate
// proof). Prospero exposes it as __sysctl like FreeBSD (r0gdb.c:1).
extern "C" int __sysctl(int*, unsigned, void*, unsigned long*, void*, unsigned long);

extern "C" void* dlsym(void*, const char*);

// ---------------------------------------------------------------------------------
// The kekcall trampoline, verbatim shape from ps5-kstuff/main.c:297:
// the 7th argument (first stack slot after the 6 register args) carries the
// FULL composed number (sub<<32 | 0x27); load it into RAX and jump to the
// syscall instruction inside the getpid wrapper (dlsym(RTLD_NEXT,"getpid")+7).
// The 6 payload args are already in RDI/RSI/RDX/RCX/R8/R9 per the SysV
// calling convention this asm receives them in.
// ---------------------------------------------------------------------------------
asm(".section .text\n"
    ".globl RC_kekcall\n"
    "RC_kekcall:\n"
    "mov 8(%rsp), %rax\n"
    "jmp *RC_p_kekcall(%rip)\n");

extern "C" uint64_t RC_kekcall(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f, uint64_t nr);

// Sub-number constants (main.c:301-306; the composition is the full 64-bit nr).
constexpr uint64_t KB_CHECK = 0xffffffff00000027ull;

static void* RC_p_kekcall = nullptr;

static bool kb_resolve_gate()
{
    if (RC_p_kekcall)
    {
        return true;
    }
    // dlsym((void*)0x1, "getpid") == RTLD_NEXT lookup through libkernel;
    // +7 lands on the wrapper's `syscall` instruction (main.c:1334).
    void* const getpid_wrapper = dlsym(reinterpret_cast<void*>(0x1), "getpid");
    if (!getpid_wrapper)
    {
        return false;
    }
    RC_p_kekcall = static_cast<char*>(getpid_wrapper) + 7;
    return true;
}

namespace RC::KernelBridge
{
    auto r0_firmware_check() -> void
    {
        // Gate proof: one CHECK kekcall. Resident => 0; not resident =>
        // parent pid (nonzero). Unknown-sub degradation cannot apply to
        // CHECK (0xffffffff is implemented in every lite release).
        if (!kb_resolve_gate())
        {
            std::fprintf(stdout, "[KB] R0 skipped reason=no-gadget dlsym-getpid-failed\n");
            std::fflush(stdout);
            return;
        }
        const uint64_t check = RC_kekcall(0, 0, 0, 0, 0, 0, KB_CHECK);
        if (check != 0)
        {
            std::fprintf(stdout, "[KB] R0 skipped reason=not-resident check=0x%llx\n",
                         static_cast<unsigned long long>(check));
            std::fflush(stdout);
            return;
        }
        // Value: plain own-pid sysctl, mib {1,46} (r0gdb_get_fw_version verbatim).
        int mib[2] = {1, 46};
        unsigned long size = sizeof(mib);
        unsigned int version = 0;
        __sysctl(mib, 2, &version, &size, nullptr, 0);
        const unsigned int firmware = version >> 16;
        const bool pass = (firmware == 0x1360u);
        std::fprintf(stdout, "[KB] R0 check=resident fw=0x%x expect=0x1360 verdict=%s\n",
                     firmware, pass ? "PASS" : "FAIL");
        std::fflush(stdout);
    }
} // namespace RC::KernelBridge

// R1/R2 refuse stubs: declared for the wiring surface, never implemented
// without a Claude-reviewed spec (PHASEB-PLAN R-ladder law).
namespace RC::KernelBridge
{
    auto r1_refused_not_implemented() -> void
    {
        std::fprintf(stdout, "[KB] R1 not-implemented-in-this-TU (needs Claude-reviewed spec)\n");
        std::fflush(stdout);
    }
    auto r2_refused_not_implemented() -> void
    {
        std::fprintf(stdout, "[KB] R2 not-implemented-in-this-TU (needs Claude-reviewed spec)\n");
        std::fflush(stdout);
    }
}

// Compile-time ABI smoke: the composed-number encoding is a pure shift-or
// of the sub constants — assert the exact composition the kernel matches.
static_assert((KB_CHECK >> 32) == 0xffffffffu, "CHECK sub must compose to 0xffffffff");
static_assert((KB_CHECK & 0xffffffffu) == 0x27u, "gate syscall must be getppid(0x27)");

#else // gate OFF: zero code

// Keep the TU translation-unit-valid in every ungated build (the header
// declares nothing then; this translation is empty by design).

#endif
