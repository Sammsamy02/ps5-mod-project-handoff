// sys_mprotect_shim.h — worker-code-anchor-reads (team 2026-10-04).
//
// THE mprotect MECHANISM (controller-ordered mprotect-FIRST design).
//
// PROVENANCE (verified this session against the PINNED SDK v0.43 — see
// CLAIM.json verification_findings):
//   - syscall number: IGCAR_SYS_MPROTECT, parsed at BUILD TIME from the
//     pinned SDK's own target/include/sys/syscall.h:82 (`#define
//     SYS_mprotect 74`) and stamped into generated/ingame_config.h. It is
//     NEVER hardcoded in this file.
//   - PROT_* constants: IGCAR_PROT_READ / IGCAR_PROT_EXEC from the pinned
//     SDK's target/include/sys/mman.h:52-55 (READ=0x01, EXEC=0x04; the
//     VMMAP prot=0x4 IS PROT_EXEC in this numbering).
//   - signature/ABI: int mprotect(const void *, size_t, int), FreeBSD
//     x86-64 syscall ABI (number in rax; args rdi, rsi, rdx; returns 0 or
//     positive errno with CF set on error — captured via setc).
//
// WHY A SHIM AND NOT AN IMPORT: sceKernelMprotect has NO verbatim row in
// the frozen 191-import contract, NO declaration in the pinned SDK, and NO
// observed row in the retained libkernel export metadata (all verified;
// the coordinator-cited windows-candidate mprotect is HOST-POSIX, not a
// console import). A fabricated import row is exactly the v1 silent-load-
// rejection suspect class. The shim introduces NO import identity: the
// 7-verbatim-import packaging is preserved EXACTLY. Free numbering-
// preservation evidence: the console-proven lineage successfully calls
// plain FreeBSD wrappers getpid(20) and sysctl(202).
//
// RESIDUAL RISK (controller sign-off required before staging): the kernel
// syscall NUMBER cannot be verified on-host. If renumbered on this console,
// the call most likely refuses (recorded, NO read happens — the safety rule
// "no mprotect success => no read" holds by construction).
//
// x86-64 ONLY. Returns 0 on success, else negative errno (FreeBSD errno).
#ifndef IGCAR_SYS_MPROTECT_SHIM_H
#define IGCAR_SYS_MPROTECT_SHIM_H

#include <stdint.h>
#include <stddef.h>

static inline long igcar_sys_mprotect(const void *addr, size_t len, int prot) {
    uint64_t ret;
    uint8_t cf;
    __asm__ volatile(
        "syscall\n\t"
        "setc %b1\n\t"
        : "=a"(ret), "=q"(cf)
        : "a"((uint64_t)IGCAR_SYS_MPROTECT), "D"(addr), "S"((uint64_t)len),
          "d"((uint64_t)prot)
        : "rcx", "r11", "memory");
    return cf ? -(long)ret : (long)ret;
}

#endif  // IGCAR_SYS_MPROTECT_SHIM_H
