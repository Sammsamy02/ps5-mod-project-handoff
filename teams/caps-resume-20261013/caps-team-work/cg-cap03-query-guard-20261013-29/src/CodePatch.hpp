/*
 * cap03/src/CodePatch.hpp — Capability #3: in-process code patching.
 *
 * PROPOSAL ONLY. Written 2026-10-13, Mac-only. Never built, never run on a
 * console, never authorized for console execution without the explicit
 * per-step user OK and the 3-restart discipline (PS5-MODDING-MISSION.md,
 * PHASE B; AGENTS.md PS5 rules).
 *
 * Design source of truth: ../DESIGN.md (protocol P1-P4, steps 1-8, U1-U6).
 * Verdict being implemented: "SOURCE-BACKED CANDIDATE; TARGET BEHAVIOR
 * UNVERIFIED" (cap01-03/PROVIDER-ROUTE-VERDICT-V2.md section 3).
 *
 * Deviations from the weak dbg.c precedent (gdb_stub/dbg.c:393-448), all
 * deliberate:
 *   - mprotect return value is CHECKED at every call (dbg.c:393-396 is
 *     `static void` and discards it).
 *   - The ORIGINAL protection is captured in pre-flight and RESTORED, with
 *     a re-query restoration proof (dbg.c leaves the page RWX forever).
 *   - Any failure at any step reverts and fails loudly; a half-applied
 *     patch is never reported as success.
 */

#ifndef CAP03_CODE_PATCH_HPP
#define CAP03_CODE_PATCH_HPP

#include <stddef.h>
#include <stdint.h>

namespace cap03 {

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

/* Largest patch this module accepts. 64 bytes is enough for the planned
 * acceptance rung (a "harmless in-game patch") and caps the revert window.
 * A 64-byte span crosses at most 2 pages at the 16 KiB PS5 page size
 * (machine/param.h: PAGE_SHIFT 14 under __SCE__). */
static const size_t kMaxPatchBytes = 64;

/* Hard cap on pages touched by one patch (defensive; 2 is the real max). */
static const int kMaxPatchPages = 4;

/* Flag bits for patch_code_ex / unpatch_code_ex. */
enum PatchFlags {
    /* P1 liveness: kekcall CHECK (0xffffffff00000027) must return 0
     * before anything is opened (verdict section 3b, precondition P1).
     * Default for patch_code(). */
    kFlagRequireLiveness = 1u << 0,
    /* Diagnostic escape hatch: skip the CHECK. Only justified when the
     * caller already measured liveness separately and wants the mechanics
     * test to run regardless (PatchDiagnostic does this on its OWN text;
     * never use this flag against game memory). Mutually exclusive with
     * kFlagRequireLiveness. */
    kFlagSkipLiveness    = 1u << 1
};

/* ------------------------------------------------------------------ */
/* Result codes                                                        */
/* ------------------------------------------------------------------ */

enum class PatchResult : int {
    kOk = 0,

    /* Argument / state validation (nothing was modified). */
    kBadArgs = 100,          /* null pointer, zero length, etc.          */
    kBadFlags,               /* liveness flags conflict                  */
    kTooLarge,               /* length > kMaxPatchBytes                  */
    kMixedProtection,        /* span pages do not share one protection   */
    kBackupInvalid,          /* unpatch: backup absent / not valid       */
    kNotPatchedState,        /* unpatch: target bytes != recorded patch  */

    /* Protocol step failures (nothing was modified). */
    kQueryFailed = 200,      /* pre-flight protection query failed (P2)  */
    kLivenessFailed,         /* kekcall CHECK non-zero / unreachable(P1) */
    kMprotectFailed,         /* OPEN: mprotect rc != 0 (step 3)          */
    kVerifyOpenFailed,       /* VERIFY-OPEN: re-query not writable (st4) */
    kReadbackMismatch,       /* WRITE readback byte mismatch (step 5)    */
    kAlreadyApplied,         /* target already contains replacement      */

    /* Restoration failures (patch was applied or page open; state is
     * recorded in the log and must be treated as DIRTY by the caller). */
    kRestoreWriteFailed = 300,   /* rewrite of original bytes failed      */
    kRestoreMprotectFailed,      /* restore mprotect rc != 0 (step 7b)    */
    kRestoreVerifyFailed,        /* re-query != original prot (step 7c)   */
    kRestoreQueryFailed          /* final re-query itself failed           */
};

const char* patch_result_str(PatchResult r);

/* ------------------------------------------------------------------ */
/* Logging                                                             */
/* ------------------------------------------------------------------ */

/* One contiguous log line per protocol step. Default sink is printf.
 * Lines are tagged: [CAP3][PATCH] / [CAP3][UNPATCH] / [CAP3][REVERT] /
 * [CAP3][DIAG], and carry step id, rc, errno, protections and capped hex
 * bytes (verdict section 3b, step 8 LOG CONTRACT). */
typedef void (*PatchLogFn)(const char* line, void* user);

void set_patch_log_sink(PatchLogFn fn, void* user);

/* Log one hex line for n bytes at addr, capped at 16 bytes. Public so the
 * diagnostic and future callers use the identical format. */
void log_hex_bytes(const char* tag, const void* addr, size_t n);

/* ------------------------------------------------------------------ */
/* Query / liveness primitives                                         */
/* ------------------------------------------------------------------ */

struct SpanProtection {
    const void* start;   /* reported mapping start (may be NULL)  */
    const void* end;     /* reported mapping end   (may be NULL)  */
    int         prot;    /* raw protection bits (R=1 W=2 X=4?)    */
};

/* Protection decode helper. Bit meanings R=1 W=2 X=4 are assumed to match
 * FreeBSD PROT_* (sys/mman.h:52-55); this is exactly the U3 caveat. The
 * function is honest about it: bit 0x8 and any unknown bits show as '?'. */
void prot_decode(int prot, char out[8]);

/* Primary query: sceKernelQueryMemoryProtection(addr, &start, &end, &prot).
 * Declared locally because the pinned SDK v0.43 headers do not declare it;
 * the 4-scalar-pointer signature is the public PS4-family one and the
 * symbol IS exported by the pinned libkernel.so (nm -D verified). The
 * signature itself is part of U3 until console-verified. */
PatchResult query_span_protection(const void* addr, SpanProtection* out);

/* Corroborating query: sceKernelVirtualQuery into a locally-declared info
 * struct. Layout is PS4-derived PUBLIC KNOWLEDGE, NOT console-verified for
 * PS5 (U3). Returns kOk only if the call rc == 0 AND the reported
 * start/end bracket addr (a layout mismatch shows up as the bracket check
 * failing, never as silent garbage). Non-fatal by design: callers log the
 * outcome and continue on kQueryFailed. */
PatchResult query_span_protection_vq(const void* addr, SpanProtection* out);

/* P1 liveness probe: kekcall CHECK sub-op 0xffffffff00000027 via the exact
 * debug-reader.c:23-46 ABI (syscall with the sub-op in RAX). Returns 0 iff
 * kstuff reports alive. Any other value, including a plain getppid result
 * (not hooked) or a fault, means NOT-ALIVE. Never retry on failure. */
int kekcall_check(void);

/* ------------------------------------------------------------------ */
/* Backup record                                                       */
/* ------------------------------------------------------------------ */

struct PatchBackup {
    uint32_t    magic;              /* kBackupMagic when valid           */
    uint32_t    page_count;         /* pages in the aligned span (1..4)  */
    void*       target;             /* exact patch target                */
    size_t      length;             /* patch length                      */
    void*       page_base;          /* first page of the aligned span    */
    size_t      span_len;           /* aligned span length               */
    int         orig_prot[kMaxPatchPages];   /* per-page raw protection   */
    uint8_t     original_bytes[kMaxPatchBytes];
    uint8_t     replacement_bytes[kMaxPatchBytes];
    uint8_t     valid;
};

static const uint32_t kBackupMagic = 0x43415033u; /* 'CAP3' */

/* ------------------------------------------------------------------ */
/* The protocol                                                        */
/* ------------------------------------------------------------------ */

/* Extended patch: full protocol (DESIGN.md steps 1-8), fills backup for a
 * later unpatch_code_ex. flags: kFlagRequireLiveness (default) or
 * kFlagSkipLiveness. On ANY failure: REVERT-ONLY runs, the failure is
 * logged loudly, and a failure code is returned — never continue with a
 * half-applied patch. */
PatchResult patch_code_ex(void* target, const void* replacement,
                          size_t length, PatchBackup* backup_out,
                          unsigned flags);

/* Task-required entry point: defaults to kFlagRequireLiveness and records
 * the backup in the module slot consumed by unpatch_code(). */
PatchResult patch_code(void* target, const void* replacement,
                       size_t length);

/* Extended unpatch: verify current bytes == recorded replacement, rewrite
 * the ORIGINAL bytes through the same OPEN/VERIFY/WRITE/READBACK path,
 * then restore the ORIGINAL protection and PROVE restoration by re-query
 * (verdict section 3b, step 7; this is the proof dbg.c never supplies).
 * One retry on the final protection query before reporting
 * kRestoreVerifyFailed (verdict 7c). Clears backup validity on success. */
PatchResult unpatch_code_ex(PatchBackup* backup, unsigned flags);

/* Task-required entry point: consumes the backup recorded by the most
 * recent successful patch_code() in this process. `original` must be the
 * caller's copy of the original bytes (compare-before-write; the module
 * independently verifies against its own recorded copy). Refuses with
 * kBackupInvalid if no patch_code() has succeeded yet — it never guesses
 * the original protection. */
PatchResult unpatch_code(void* target, const void* original,
                         size_t length);

/* ------------------------------------------------------------------ */
/* RAII scope                                                          */
/* ------------------------------------------------------------------ */

/* Applies the patch on construction, auto-reverts on destruction. If the
 * revert fails the destructor logs a loud SCOPE-LEAK line and leaves
 * status() at the restore-failure code — the caller owns the dirty state.
 * The destructor never throws and never retries more than the protocol's
 * one restore retry per unpatch_code_ex call: an explicit revert() that
 * failed will NOT be retried again by the destructor (no blind retries
 * past the verdict-7c bound). */
class PatchScope {
public:
    PatchScope(void* target, const void* replacement, size_t length,
               unsigned flags = kFlagRequireLiveness);
    ~PatchScope();

    bool          ok() const;        /* patch applied AND not yet reverted */
    PatchResult   status() const;    /* last protocol result               */
    PatchResult   revert();          /* early manual revert (idempotent)   */
    const PatchBackup& backup() const { return backup_; }

private:
    PatchScope(const PatchScope&);            /* non-copyable */
    PatchScope& operator=(const PatchScope&); /* non-assignable */

    PatchBackup backup_;
    PatchResult status_;
    bool        armed_;
    bool        revert_attempted_;
};

} /* namespace cap03 */

#endif /* CAP03_CODE_PATCH_HPP */
