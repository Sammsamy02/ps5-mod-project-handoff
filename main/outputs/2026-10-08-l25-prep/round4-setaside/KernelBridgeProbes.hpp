// [KB] Kernel-bridge observe-only probes (capability #1, R0 rung).
// DEFAULT OFF: the entire TU compiles to nothing unless
// MODBRIDGE_KB_PROBES is defined — today's builds carry zero KB code
// (kstuff-lite-v1.11-VERIFIED.md law; the GP audit confirmed no build
// config defines this).
//
// ABI grounded in the pinned v1.11 source (outputs/kstuff-lite-pinned/,
// extracted ps5-kstuff/main.c:297-308, prosper0gdb/r0gdb.c r0gdb_get_fw_version):
//   - kekcall gate: syscall 0x27 (getppid) with the sub-number composed
//     into RAX[63:32]; caller-side trampoline loads the composed number
//     from the 7th argument slot and jumps to the syscall instruction
//     inside the getpid wrapper: dlsym(RTLD_NEXT,"getpid")+7.
//   - Sub constants (main.c:301-306): 0x027 passthrough, 0x100000027
//     READ_DR, 0x200000027 WRITE_DR, 0x300000027 RDMSR, 0x500000027
//     REMOTE_SYSCALL, 0xffffffff00000027 CHECK.
//   - CHECK polarity (main.c:1336): returns 0 when RESIDENT, parent pid
//     when not (the caller's own "already loaded" test).
//   - Unknown sub-numbers silently degrade to a real getppid — support is
//     NEVER probed by return value.
//   - R0 firmware value: plain own-pid sysctl, mib {1,46}, 32-bit
//     version word, firmware = version>>16 (r0gdb.c r0gdb_get_fw_version,
//     verbatim shape).
// Scope law: R0 ONLY (CHECK gate proof + the firmware read). R1/R2 are
// declared and refuse loudly; every rung beyond R0 needs a Claude-reviewed
// spec first. No writes anywhere in this TU, ever.
#pragma once

#include <cstdint>

#if defined(__PROSPERO__) && defined(MODBRIDGE_KB_PROBES)

namespace RC::KernelBridge
{
    // The one entry the initializer MAY call (under the compile gate and
    // only from a reviewed console session): runs the R0 rung and logs
    // exactly one verdict line. Never throws; never writes anything.
    auto r0_firmware_check() -> void;
}

#endif
