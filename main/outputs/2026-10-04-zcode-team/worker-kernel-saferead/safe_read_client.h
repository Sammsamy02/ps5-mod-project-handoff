// safe_read_client.h — userspace client for the safe-read kernel service,
// for use inside the UE4SS PRX running in the GAME process.
//
// STATUS: research prototype header, never compiled. The kekcall shim follows
// the exact register contract of the CRT's __sp_kekcall shim
// (tools/SharpProspero.Link/PayloadCrtEmitter.cs lines 2863-2900) and the
// syscall-gadget derivation documented in __sp_crt_syscall_init
// (sceKernelDlsym("getpid") + 0xA).
//
// Channel recap (see safe_read_kernel.c section 6): kstuff hooks
// sysent[SYS_getppid=39] globally. A normal getppid passes through. A kekcall
// packs the call number into RAX[63:32]. Because the hook is global, the game
// process reaches it with no privileges at all.
//
// New kekcall numbers this client assumes (must be added to
// KernelModuleEmitter.cs EmitKekcallHandler and rebuilt — NOT present in the
// currently resident module):
//   nr = 7  safe_read:  a1 = struct sr_request* (caller memory, copyin'd)
//                       retval >= 0: bytes copied to req->buf
//                       retval < 0 : -SR_ERR_*
//   nr = 8  safe_probe: a1 = struct sr_probe_request*
//                       retval: SR_PROBE_* codes
//   nr = -1 ping (already exists): returns 0 when the module is alive.
//
// Until the rebuilt module is deployed, kstuff_safe_read_alive() answers
// "is SOME kstuff present", not "does it have safe-read" — always gate on
// error returns (ENOSYS=78 means: old module, rebuild required).

#ifndef SAFE_READ_CLIENT_H
#define SAFE_READ_CLIENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- raw syscall gadget ----------------------------------------------------
//
// The CRT resolves the syscall instruction by taking the resolved address of
// libkernel's getpid and adding 0xA (the offset of the `syscall` instruction
// inside it on the supported firmware family). From the PRX we do the same
// through sceKernelDlsym. If the PRX already has a raw-syscall path, use it.
//
// typedef of the dlsym we get from the PRX loader environment:
typedef int (*dlsym_fn)(int handle, const char *name, void **out);

// Set once at PRX startup: kstuff_client_init(dlsym, 0x1) — handle 0x1 is the
// process's own image; fall back to 0x2001 (OS library) exactly like the CRT.
int kstuff_client_init(dlsym_fn dlsym);

// ---- kekcall shim ----------------------------------------------------------
//
// Register contract ([CRT] __sp_kekcall):
//   eax = nr (zero-extended); then args shift one slot down:
//   RDI=a1, RSI=a2, RDX=a3, R10=a4, R8=a5, R9=a6
//   rax = (nr << 32) | 0x27   (SYS_getppid = 39)
//   call [gadget]
// Constraints: the kernel-side handler reads a1 (target pid etc.) from RDI and
// the request pointer from RCX per nr=5's documented ABI; the packed request
// structs below are passed as a1, matching the nr=1/nr=5 copyin pattern.

long kstuff_kekcall(int nr, long a1, long a2, long a3,
                    long a4, long a5, long a6);

#if defined(__x86_64__) && !defined(KSTUFF_NO_ASM)
// Direct asm variant — no gadget needed IF the console allows the syscall
// instruction from the PRX's mapped text. The CRT avoids inline syscall by
// convention (gadget reuse); verify on-device before relying on this path.
// The gadget path above is the conservative default.
#define KSTUFF_HAVE_DIRECT_ASM 1
#endif

// ---- service API ------------------------------------------------------------

// Negative return codes (mirror of the kernel-side definitions).
#define KSR_ERR_NOENT     (-10)   // pid not found / process gone
#define KSR_ERR_UNMAPPED  (-11)   // address not covered / page not present
#define KSR_ERR_PROT      (-12)   // mapped but not readable
#define KSR_ERR_LEN       (-13)   // zero or oversized length
#define KSR_ERR_RANGE     (-14)   // kernel-range VA or copyout fault
#define KSR_ERR_STRUCT    (-15)   // kernel structure unreadable (transient)
#define KSR_ERR_ENOSYS    (-78)   // kekcall number unknown: OLD kstuff build
#define KSR_ERR_NOCHANNEL (-100)  // client not initialized / gadget missing

// Probe codes.
#define KSR_PROBE_READABLE 1
#define KSR_PROBE_NOREAD   0
#define KSR_PROBE_UNMAPPED (-1)
#define KSR_PROBE_FAULT    (-2)
#define KSR_PROBE_BADARGS  (-3)

#define KSR_MAX_LEN 0x10000u      // 64 KiB per call, keep in sync with kernel

struct ksr_request {              // kekcall nr=7 — keep layout in sync with
    uint32_t pid;                 // safe_read_kernel.c struct sr_request
    uint32_t _pad0;
    uint64_t addr;                // target user VA in `pid`
    uint64_t len;                 // <= KSR_MAX_LEN
    uint64_t buf;                 // OUR buffer (same process) to receive data
};

struct ksr_probe_request {        // kekcall nr=8
    uint32_t pid;
    uint32_t _pad0;
    uint64_t addr;
    uint64_t len;
};

// Liveness: 0 when a kstuff kernel module answers kekcalls (existing nr=-1).
long kstuff_safe_read_alive(void);

// bool safe_probe(uint64_t addr, uint64_t len) — is OUR range mapped+readable?
static inline int safe_probe(uint64_t addr, uint64_t len)
{
    return kstuff_safe_read_probe(0 /* caller pid */, addr, len);
}

// bool safe_probe_other(pid_t pid, ...) — probe another process's range.
int kstuff_safe_read_probe(int pid, uint64_t addr, uint64_t len);

// int safe_read(uint64_t addr, void *buf, uint64_t len)
// Returns len (>0) on success, negative KSR_ERR_* on failure. Never faults:
// every page is validated in the target's page tables before being read.
static inline int safe_read(uint64_t addr, void *buf, uint64_t len)
{
    return kstuff_safe_read_other(0 /* caller pid */, addr, buf, len);
}

// int safe_read_other(pid_t target_pid, uint64_t addr, void *buf, uint64_t len)
int kstuff_safe_read_other(int target_pid, uint64_t addr, void *buf,
                           uint64_t len);

#ifdef __cplusplus
}
#endif

#endif // SAFE_READ_CLIENT_H

// ============================================================================
// IMPLEMENTATION SECTION — compile as part of the UE4SS PRX (single file,
// no dependencies beyond sceKernelDlsym availability at init time).
// ============================================================================
#ifdef KSTUFF_CLIENT_IMPLEMENTATION

static long (*g_syscall_gadget)(void);   // addr of `syscall` insn (getpid+0xA)
static dlsym_fn g_dlsym;

int kstuff_client_init(dlsym_fn dlsym)
{
    g_dlsym = dlsym;
    if (!dlsym) return KSR_ERR_NOCHANNEL;

    void *fn = 0;
    const char getpid_name[] = "getpid";
    if (dlsym(0x1, getpid_name, &fn) != 0 || fn == 0) {
        if (dlsym(0x2001, getpid_name, &fn) != 0 || fn == 0)
            return KSR_ERR_NOCHANNEL;
    }
    g_syscall_gadget = (long (*)(void))((char *)fn + 0xA);
    return 0;
}

long kstuff_kekcall(int nr, long a1, long a2, long a3,
                    long a4, long a5, long a6)
{
    if (!g_syscall_gadget) return KSR_ERR_NOCHANNEL;

    long ret;
    __asm__ volatile (
        "movl  %%edi, %%eax\n\t"      // eax = nr (zero-extend: upper 32 clear)
        "movq  %%rsi, %%rdi\n\t"      // a1 -> RDI
        "movq  %%rdx, %%rsi\n\t"      // a2 -> RSI
        "movq  %%rcx, %%rdx\n\t"      // a3 -> RDX
        "movq  %%r8,  %%r10\n\t"      // a4 -> R10 (FreeBSD arg4)
        "movq  %%r9,  %%r8\n\t"       // a5 -> R8
        "movq  a6_,   %%r9\n\t"       // a6 -> R9   (filled below)
        "shlq  $32, %%rax\n\t"        // nr << 32
        "orq   $39,  %%rax\n\t"       // | SYS_getppid
        "callq *gadget_\n\t"
        : "=a"(ret)
        : "D"(nr), "S"(a1), "d"(a2), "c"(a3), "r"(a4), "r"(a5)
        : "r10", "r11", "memory"
    );
    return ret;
}

// The two asm-referenced symbols are provided through local wrappers because
// GCC extended asm cannot name C locals directly in mid-sequence; in the real
// PRX build, write the shim in a .S file or use top-level asm. The sequence
// above is the contract; the .S version is 15 instructions:
//
//   kstuff_kekcall:
//       movl   %edi, %eax
//       movq   %rsi, %rdi
//       movq   %rdx, %rsi
//       movq   %rcx, %rdx
//       movq   %r8,  %r10
//       movq   %r9,  %r8
//       movq   8(%rsp), %r9        // 7th arg (a6)
//       shlq   $32, %rax
//       orq    $39, %rax
//       call   *g_syscall_gadget(%rip)
//       ret
//
// (identical opcode-by-opcode to the CRT's __sp_kekcall bytes.)

long kstuff_safe_read_alive(void)
{
    return kstuff_kekcall(-1, 0, 0, 0, 0, 0, 0);   // existing kekcall nr=-1
}

int kstuff_safe_read_other(int target_pid, uint64_t addr, void *buf,
                           uint64_t len)
{
    if (!g_syscall_gadget) return KSR_ERR_NOCHANNEL;
    if (len == 0 || len > KSR_MAX_LEN) return KSR_ERR_LEN;

    struct ksr_request req;
    req.pid = (uint32_t)target_pid;
    req._pad0 = 0;
    req.addr = addr;
    req.len = len;
    req.buf = (uint64_t)(uintptr_t)buf;

    return (int)kstuff_kekcall(7, (long)(uintptr_t)&req, 0, 0, 0, 0, 0);
}

int kstuff_safe_read_probe(int target_pid, uint64_t addr, uint64_t len)
{
    if (!g_syscall_gadget) return KSR_ERR_NOCHANNEL;
    if (len == 0 || len > KSR_MAX_LEN) return KSR_PROBE_BADARGS;

    struct ksr_probe_request req;
    req.pid = (uint32_t)target_pid;
    req._pad0 = 0;
    req.addr = addr;
    req.len = len;

    return (int)kstuff_kekcall(8, (long)(uintptr_t)&req, 0, 0, 0, 0, 0);
}

#endif // KSTUFF_CLIENT_IMPLEMENTATION
