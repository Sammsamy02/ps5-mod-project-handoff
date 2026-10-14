// KekcallConsoleShim.hpp — console-side transport for KekcallServices.hpp.
// staged by worker-ue4ss-kernel (2026-10-06). RESEARCH PROTOTYPE: never
// compiled on host, never run on console. No console contact, no game claims.
//
// What this adapts: worker-kernel-saferead/safe_read_client.h's client
// implementation resolves the raw `syscall` gadget as
// sceKernelDlsym("getpid") + 0xA (the CRT's __sp_kekcall recipe) and packs
// the kekcall number into RAX[63:32] over SYS_getppid. That header's inline
// asm cannot compile on a host and its C implementation section carries the
// documented 15-instruction .S contract; until the PRX build gains the .S
// file, this adapter provides the same ABI as a weakly-linked extern so the
// final link can bind either (a) the .S shim, or (b) a C translation unit
// compiled from safe_read_client.h with KSTUFF_CLIENT_IMPLEMENTATION.
//
// Usage (console TUs only):
//     #include "KekcallConsoleShim.hpp"
//     ksrv::init(kekcall_console::vtable());   // idempotent, cheap
//
// The shim itself performs NO syscall on include/init; only kekcall traffic
// (nr=-1 ping first) touches the channel. On a boot with no kstuff module
// resident the ping simply fails and the facade stays Unknown/NoChannel —
// every consumer then runs its legacy path byte-for-byte.

#pragma once

#if defined(__PROSPERO__)

#include "KekcallServices.hpp"

extern "C"
{
    // libkernel_surface (PRX loader environment; handle 0x1 = own image,
    // 0x2001 = OS library fallback — the CRT recipe, safe_read_client.h).
    int sceKernelDlsym(int handle, const char *name, void **out);

    // The __sp_kekcall-identical shim. Provided by ONE of:
    //   - the PRX build's kstuff_kekcall.S (preferred; opcode-identical to
    //     the CRT bytes, safe_read_client.h lines 188-206), or
    //   - safe_read_client.h compiled with KSTUFF_CLIENT_IMPLEMENTATION and
    //     kstuff_client_init(sceKernelDlsym) called once at PRX startup.
    long kstuff_kekcall(int nr, long a1, long a2, long a3, long a4, long a5, long a6);
}

namespace kekcall_console
{
    inline long shim(int nr, long a1, long a2, long a3, long a4, long a5, long a6)
    {
        return kstuff_kekcall(nr, a1, a2, a3, a4, a5, a6);
    }

    inline const ksrv::VTable *vtable()
    {
        static const ksrv::VTable vt{&shim};
        return &vt;
    }
}

#endif // __PROSPERO__
