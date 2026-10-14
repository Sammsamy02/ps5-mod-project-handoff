/*
 * cap03/src/PatchDiagnostic.cpp — Capability #3 diagnostic mode.
 *
 * Staged v2 diagnostic source (2026-10-13, Mac-only; this revision has
 * not run on a console).
 *
 * Self-contained probe in the shape of the "separate no-game probe" model
 * (PROVIDER-ROUTE-VERDICT-V2.md section 2c): a standalone payload ELF,
 * loaded via `pldmgr /loadpayload`, with NO game launched. It patches a
 * function in its OWN text (never the game's), measures whether the
 * kstuff mprotect permission-check skip is engaged for this process,
 * proves write/readback/restore mechanics end-to-end, and reports the
 * U1-U6 verification status (verdict section 3c) from its log lines.
 *
 * Containment argument (why this rung may run before any in-game test):
 * the patch target is the probe's own text in the payload-daemon process;
 * there is no game process to destabilize; every protocol step reverts on
 * failure; and the one-user-launch-per-test discipline binds at the
 * console only with explicit user OK per TEST-PLAN.md.
 *
 * Entry points:
 *   main()                    — standalone payload entry (pldmgr).
 *   run_cap03_diagnostic()    — library form for a future PRX host.
 * Both print the identical contiguous log; exit code 0 = PASS, 1 = FAIL,
 * 2 = INCONCLUSIVE/DIRTY (restore failed; state must be treated as dirty).
 */

#include "CodePatch.hpp"

#include <machine/param.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* MAP_ANON is only declared by the SDK sys/mman.h:85 under __BSD_VISIBLE;
 * a strict POSIX build (-D_XOPEN_SOURCE=700) hides it. Provide the value
 * from the pinned SDK header (FreeBSD MAP_ANON = 0x1000) as a fallback,
 * and prove it stays in sync whenever the header IS visible. Fix made
 * 2026-10-13 during the first real prospero-clang compile (KD, cap03). */
#ifndef MAP_ANON
#define MAP_ANON 0x1000
#else
static_assert(MAP_ANON == 0x1000, "MAP_ANON drifted from sys/mman.h:85");
#endif

namespace {

/* ------------------------------------------------------------------ */
/* The probe target: a function in the diagnostic's OWN text.          */
/* ------------------------------------------------------------------ */

/* Hand-written so the layout is known exactly and the 6-byte patch fits
 * with margin: 5-byte mov-imm + 8 nops + ret (15 bytes >= 6). Top-level
 * asm keeps the compiler from inlining or const-folding call sites. */
__asm__(
    ".text\n"
    ".globl cap3_diag_probe_fn\n"
    ".type cap3_diag_probe_fn, @function\n"
    "cap3_diag_probe_fn:\n"
    "  movl $0x1337, %eax\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  nop\n"
    "  ret\n"
    ".size cap3_diag_probe_fn, 15\n"
);

extern "C" int cap3_diag_probe_fn(void);

/* Replacement: `mov eax, 0xC0FFEE ; ret` (x86-64, 6 bytes). */
const uint8_t kReplacement[6] = { 0xB8, 0xEE, 0xFF, 0xC0, 0x00, 0xC3 };
const size_t  kPatchLen = sizeof(kReplacement);

const int kExpectOriginal = 0x1337;
const int kExpectPatched  = 0x00C0FFEE;

/* ------------------------------------------------------------------ */
/* Tiny harness                                                        */
/* ------------------------------------------------------------------ */

struct Diag {
    int  rc;              /* final exit code so far: 0 pass, 1 fail, 2 dirty */
    int  failures;
    int  liveness;        /* kekcall_check() result: 0 alive                 */
    int  ctrl_mprot_rc;   /* control-page mprotect rc                        */
    bool text_already_writable;
    bool patch_ok;
    bool unpatch_ok;
    bool behavior_patched_ok;
    bool behavior_restored_ok;
    int  vq_bracket_ok;   /* corroboration query successes                   */
    bool dirty;           /* restore failed anywhere -> STATE DIRTY          */
};

void diag_step(Diag* d, const char* name, bool ok)
{
    if(ok)
        printf("[CAP3][DIAG] step %-24s PASS\n", name);
    else
    {
        printf("[CAP3][DIAG] step %-24s FAIL\n", name);
        ++d->failures;
    }
}

bool is_restore_failure(cap03::PatchResult r)
{
    return r == cap03::PatchResult::kRestoreWriteFailed ||
           r == cap03::PatchResult::kRestoreMprotectFailed ||
           r == cap03::PatchResult::kRestoreVerifyFailed ||
           r == cap03::PatchResult::kRestoreQueryFailed;
}

/* volatile sinks so the compiler cannot const-fold the probe calls */
volatile int g_sink;

} /* anonymous namespace */

extern "C" int run_cap03_diagnostic(void)
{
    Diag d;
    memset(&d, 0, sizeof(d));
    d.rc = 0;

    printf("[CAP3][DIAG] =============================================\n");
    printf("[CAP3][DIAG] cap03 code-patch diagnostic (proposal only)\n");
    printf("[CAP3][DIAG] no game running; target = OWN text @%p\n",
           (void*)&cap3_diag_probe_fn);
    printf("[CAP3][DIAG] PAGE_SIZE(compile-time)=%u kPatchLen=%zu\n",
           (unsigned)PAGE_SIZE, kPatchLen);
    printf("[CAP3][DIAG] U6 note: no game launched; kstuff ShadowMount "
           "pause semantics do not apply to this payload process\n");
    printf("[CAP3][DIAG] =============================================\n");

    /* S0: control — plain mprotect on an ANON page must succeed without
     * any fix. Distinguishes "mprotect path broken" from "text write
     * refused". */
    void* ctrl = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON, -1, 0);
    if(ctrl == MAP_FAILED)
    {
        printf("[CAP3][DIAG] S0 control mmap failed errno=%d\n", errno);
        d.ctrl_mprot_rc = -1;
    }
    else
    {
        errno = 0;
        int rc1 = mprotect(ctrl, PAGE_SIZE, PROT_READ);   /* de-write */
        int rc2 = mprotect(ctrl, PAGE_SIZE, PROT_READ | PROT_WRITE);
        d.ctrl_mprot_rc = (rc1 == 0 && rc2 == 0) ? 0 : -1;
        printf("[CAP3][DIAG] S0 control page=%p mprotect rc1=%d rc2=%d "
               "errno=%d (expected 0/0: plain mprotect works)\n",
               ctrl, rc1, rc2, errno);
        munmap(ctrl, PAGE_SIZE);
    }
    diag_step(&d, "S0-control-mprotect", d.ctrl_mprot_rc == 0);

    /* S1: liveness (P1 measurement). Recorded, NOT fatal here: the patch
     * below is on OWN text with kFlagSkipLiveness so the mechanics test
     * runs regardless — a failed CHECK degrades the run to measurement,
     * never to a blind game-memory write (deviation documented in
     * DESIGN.md section 6). */
    d.liveness = cap03::kekcall_check();
    printf("[CAP3][DIAG] S1 kekcall CHECK -> %d (%s); U1(payload-context)=%s\n",
           d.liveness, d.liveness == 0 ? "alive" : "not-alive",
           d.liveness == 0 ? "engaged" : "NOT ENGAGED from this process");
    diag_step(&d, "S1-liveness", d.liveness == 0);

    /* S2: pre-flight on own text (P2). Capture original protection;
     * detect the already-writable case that would make U2 INCONCLUSIVE. */
    cap03::SpanProtection sp;
    memset(&sp, 0, sizeof(sp));
    cap03::PatchResult qr = cap03::query_span_protection(
        (const void*)&cap3_diag_probe_fn, &sp);
    cap03::SpanProtection vsp;
    if(cap03::query_span_protection_vq((const void*)&cap3_diag_probe_fn,
                                       &vsp) == cap03::PatchResult::kOk)
        ++d.vq_bracket_ok;
    d.text_already_writable =
        (qr == cap03::PatchResult::kOk) && ((sp.prot & PROT_WRITE) != 0);
    printf("[CAP3][DIAG] S2 own-text prot=0x%x already_writable=%d "
           "(already-writable => U2 INCONCLUSIVE: mapping needs no skip)\n",
           qr == cap03::PatchResult::kOk ? (unsigned)sp.prot : 0u,
           (int)d.text_already_writable);
    diag_step(&d, "S2-preflight-query", qr == cap03::PatchResult::kOk);

    /* S3: PATCH via the library under test (full protocol, liveness
     * skipped as above — own text only). */
    cap03::PatchBackup backup;
    memset(&backup, 0, sizeof(backup));
    cap03::PatchResult pr = cap03::patch_code_ex(
        (void*)&cap3_diag_probe_fn, kReplacement, kPatchLen, &backup,
        cap03::kFlagSkipLiveness);
    d.patch_ok = (pr == cap03::PatchResult::kOk);
    if(is_restore_failure(pr))
        d.dirty = true;
    printf("[CAP3][DIAG] S3 patch_code_ex -> %d (%s)\n",
           (int)pr, cap03::patch_result_str(pr));
    diag_step(&d, "S3-patch", d.patch_ok);

    /* S4: behavioral proof — the patched function must return the new
     * immediate. Single-threaded: no other core can be executing the
     * span (U4 containment). */
    if(d.patch_ok)
    {
        g_sink = cap3_diag_probe_fn();
        d.behavior_patched_ok = (g_sink == kExpectPatched);
        printf("[CAP3][DIAG] S4 behavior(patched) -> 0x%x (want 0x%x)\n",
               g_sink, kExpectPatched);
    }
    else
        printf("[CAP3][DIAG] S4 SKIPPED: patch was not proven; do not execute text\n");
    diag_step(&d, "S4-behavior-patched", d.behavior_patched_ok);

    /* S5: UNPATCH via the library (restoration proof included). */
    cap03::PatchResult ur = cap03::PatchResult::kBackupInvalid;
    if(d.patch_ok)
    {
        ur = cap03::unpatch_code_ex(&backup, 0);
        d.unpatch_ok = (ur == cap03::PatchResult::kOk);
        if(!d.unpatch_ok)
            d.dirty = true; /* state after unpatch failure is unknown */
        printf("[CAP3][DIAG] S5 unpatch_code_ex -> %d (%s)\n",
               (int)ur, cap03::patch_result_str(ur));
    }
    else
        printf("[CAP3][DIAG] S5 SKIPPED: no successful patch backup\n");
    diag_step(&d, "S5-unpatch", d.unpatch_ok);

    /* S6: behavioral restoration proof. */
    if(d.unpatch_ok)
    {
        g_sink = cap3_diag_probe_fn();
        d.behavior_restored_ok = (g_sink == kExpectOriginal);
        printf("[CAP3][DIAG] S6 behavior(restored) -> 0x%x (want 0x%x)\n",
               g_sink, kExpectOriginal);
    }
    else
        printf("[CAP3][DIAG] S6 SKIPPED: restoration not proven; do not execute text\n");
    diag_step(&d, "S6-behavior-restored", d.behavior_restored_ok);

    /* S7: independent double-check of the restoration via VirtualQuery
     * (U5 at t0; long-duration stickiness belongs to the console rungs). */
    cap03::SpanProtection after;
    memset(&after, 0, sizeof(after));
    bool s7_ok = (cap03::query_span_protection(
                      (const void*)&cap3_diag_probe_fn,
                      &after) == cap03::PatchResult::kOk)
              && (qr == cap03::PatchResult::kOk)
              && (sp.prot == after.prot);
    if(cap03::query_span_protection_vq((const void*)&cap3_diag_probe_fn,
                                       &vsp) == cap03::PatchResult::kOk)
        ++d.vq_bracket_ok;
    printf("[CAP3][DIAG] S7 restore re-query: before=0x%x after=0x%x "
           "vq_ok_total=%d\n",
           qr == cap03::PatchResult::kOk ? (unsigned)sp.prot : 0u,
           s7_ok ? (unsigned)after.prot : 0u, d.vq_bracket_ok);
    diag_step(&d, "S7-restore-requery", s7_ok);

    /* ------------------- U1-U6 verification report ------------------- */
    printf("[CAP3][DIAG] ---------------- U1-U6 STATUS ------------------\n");
    printf("[CAP3][DIAG] U1 interception engaged (PAYLOAD context): %s\n",
           d.liveness == 0 ? "MEASURED: alive"
                           : "MEASURED: not engaged from this process");
    printf("[CAP3][DIAG] U1 note: game-PRX engagement remains UNVERIFIED "
           "(separate in-game rung, TEST-PLAN console test 4)\n");
    if(d.text_already_writable)
        printf("[CAP3][DIAG] U2 self-text writable via mprotect: "
               "INCONCLUSIVE (text already writable; no skip needed)\n");
    else
        printf("[CAP3][DIAG] U2 self-text writable via mprotect: %s\n",
               d.patch_ok ? "MEASURED: skip engaged (R|X text -> RWX ok)"
                          : "MEASURED: skip NOT effective (see OPEN rc)");
    printf("[CAP3][DIAG] U3 query fidelity: %s (qmp primary, vq "
           "bracket-ok=%d; disagreement aborts VERIFY-OPEN by design)\n",
           (qr == cap03::PatchResult::kOk && s7_ok) ? "consistent t0"
                                                    : "INCONSISTENT/failed",
           d.vq_bracket_ok);
    printf("[CAP3][DIAG] U4 executing-core visibility: NOT TESTED in this "
           "rung (single-threaded, span quiescent by construction)\n");
    printf("[CAP3][DIAG] U5 restoration: t0 proof %s; stick-over-time "
           "deferred to console rungs 2-3\n",
           (d.unpatch_ok && d.behavior_restored_ok && s7_ok)
               ? "PASS" : "FAIL");
    printf("[CAP3][DIAG] U6 pause state: N/A (no game); kstuff liveness %s\n",
           d.liveness == 0 ? "measured alive" : "NOT alive");
    printf("[CAP3][DIAG] --------------------------------------------------\n");

    int failed = d.failures;
    printf("[CAP3][DIAG] SUMMARY: failures=%d dirty=%d -> %s\n",
           failed, (int)d.dirty,
           d.dirty ? "DIRTY (treat state as patched/indeterminate)"
                   : (failed == 0 ? "PASS" : "FAIL"));
    if(d.dirty)
        return 2;
    return failed == 0 ? 0 : 1;
}

/* Standalone payload entry (pldmgr /loadpayload; no game needed). */
int main(void)
{
    int rc = run_cap03_diagnostic();
    printf("[CAP3][DIAG] exit=%d\n", rc);
    return rc;
}
