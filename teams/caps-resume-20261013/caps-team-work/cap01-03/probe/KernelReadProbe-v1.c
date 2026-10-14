/*
 * cap01-kread-probe-v1 — standalone no-game kernel-data read probe.
 *
 * V1 FIX (2026-10-13, implements LSTAR-DIAGNOSIS.md "Recommended fix"):
 *   v0's kekcall() encoded the carry flag by NEGATING the return value, and
 *   classify_msr_result() then treated any negative int64 as an error. A
 *   successful LSTAR read returns a canonical kernel-half address (bit 63
 *   set), so v0 misclassified 100% of successful LSTAR reads as
 *   "msr_read_faulted_or_errno" (console 2026-10-13: CHECK, EFER, TSC x2,
 *   READ_DR all PASS; only LSTAR "FAIL"). kstuff-lite v1.11 itself passes
 *   args[RDI] to rdmsr() unfiltered and back-ports the value uniformly
 *   (uelf/kekcall.c:59-62, uelf/utils.c:799-812) — no kstuff-side fault is
 *   involved. V1 returns the RAW RAX plus a separate CF flag and classifies
 *   on the FLAG first, never on the value's sign; classify-failure branches
 *   now log the raw rc so any future FAIL is self-documenting. Everything
 *   else (checks, order, gating, exit codes) is unchanged from v0.
 *   v0 source is preserved byte-exact as KernelReadProbe.c; v0 console
 *   evidence as EVIDENCE-CONSOLE-20261013.log. Distinct probe ID and log
 *   path per the new-revision law: v0 evidence is never re-read as v1.
 *
 * PROVENANCE: client ABI is the verbatim inline-syscall shape of
 * kstuff-lite v1.11 ps5-kstuff/debug-reader.c:21-46 (also verified in
 * kstuff-lite-v1.11-VERIFIED.md G1-G4, K2). Sub-op semantics:
 *   ps5-kstuff/uelf/kekcall.c:19-107  (dispatch table)
 *   ps5-kstuff/uelf/utils.c:677-690   (read_dbgregs_checked: DR0-3,DR6,DR7)
 *   ps5-kstuff/uelf/utils.c:799-812   (rdmsr)
 * See DESIGN.md in this directory for the full architecture rationale.
 *
 * SCOPE (read-only by construction):
 *   - calls ONLY CHECK (sub 0xffffffff), RDMSR (sub 3), READ_DR (sub 1).
 *   - NEVER calls WRITE_DR (2), REMOTE_SYSCALL (5), PPR control (7).
 *   - performs zero kernel writes; loads no PRX; touches no game.
 *
 * STATUS: staged deliverable. Built Mac-only. It is loaded on the console
 * ONLY with the user's explicit OK (see TEST-CARD.md). Not authorized by
 * this file.
 *
 * HONESTY BOUNDARY: this probe reads kernel REGISTERS (MSRs, debug
 * registers). It does NOT read arbitrary kernel virtual addresses. An
 * arbitrary kernel-VA read needs the Phase-B KREAD kekcall sub-op in
 * kstuff-lite (see ../KERNEL-READ-ONE-ANSWER.md) — not implemented here.
 *
 * ERROR-CONVENTION ROBUSTNESS: the pinned source does not prove whether the
 * intercepted-syscall error return sets RFLAGS.CF (debug-reader.c:36 assumes
 * yes) or just leaves errno in RAX (uelf/main.c:77-81 writes regs[RAX]=err
 * for both success-with-error and the resumed path). V1 captures BOTH
 * signals and classification runs on the RAW uint64 RAX plus the separately
 * captured CF:
 *   CF set                        -> error, errno convention of debug-reader.c:36
 *   rc == 0, CF clear             -> success with zero result
 *   0 < rc < ERRNO_CLASS_MAX      -> errno-class failure (uelf/main.c:77-81
 *                                    convention) or (for CHECK) the real
 *                                    getppid parent pid; both are failures
 *                                    for this probe and the raw value is
 *                                    always logged
 *   rc >= ERRNO_CLASS_MAX         -> a real returned VALUE (MSR); bit 63 set
 *                                    (canonical kernel address, e.g. LSTAR)
 *                                    is a VALID success, never an error
 *
 * Entry: ps5-payload-sdk crt1.o _start -> main(). Plain ELF executable.
 * Output: stdout (payload-daemon log path) + own file
 *         /data/cap01-kread-probe-v1.log
 * Exit codes: 0 PASS | 2 CHECK failed | 3 evidence gate failed |
 *             1 internal error (no log produced)
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define PROBE_ID      "cap01-kread-probe-v1"
#define PROBE_LOGPATH "/data/cap01-kread-probe-v1.log"

/* Errno-class values from the kernel are far below any real MSR/TSC value. */
#define ERRNO_CLASS_MAX UINT64_C(0x1000)

/* --- kekcall client ABI (debug-reader.c:21-46 shape) ------------------- */

/* Returns the RAW RAX; *cf_error receives RFLAGS.CF. V1: the flag travels
 * separately — a successful RDMSR of LSTAR has bit 63 set in RAX and must
 * never be classified from the value's sign. */
static uint64_t kekcall(uint64_t a, uint64_t b, uint64_t c,
                        uint64_t d, uint64_t e, uint64_t f, uint64_t g,
                        int* cf_error)
{
    register uint64_t r10 __asm__("r10") = d;
    register uint64_t r8 __asm__("r8") = e;
    register uint64_t r9 __asm__("r9") = f;
    uint64_t ret;
    unsigned char is_error;

    __asm__ __volatile__(
        "syscall"
        : "=a"(ret), "=@ccc"(is_error), "+r"(r10), "+r"(r8), "+r"(r9)
        : "a"(g), "D"(a), "S"(b), "d"(c)
        : "rcx", "r11", "memory");

    if(cf_error)
        *cf_error = is_error ? 1 : 0;
    return ret;
}

/* Sub-op numbers: composed = (sub << 32) | 0x27 (SYS_getppid).
 * Table verified in uelf/kekcall.c:19-107 and VERIFIED.md K1/K2. */
#define KEKCALL_READ_DR  UINT64_C(0x0000000100000027) /* sub 1: 48B -> [a1]  */
#define KEKCALL_RDMSR    UINT64_C(0x0000000300000027) /* sub 3: ret = MSR    */
#define KEKCALL_CHECK    UINT64_C(0xffffffff00000027) /* sub -1: 0 if alive  */

/* MSRs (all universal amd64; RDMSR of an invalid MSR fails clean via the
 * kstuff gadget fault path, utils.c:799-812). */
#define MSR_TSC          UINT64_C(0x00000010)
#define MSR_EFER         UINT64_C(0xc0000080)
#define MSR_LSTAR        UINT64_C(0xc0000082)

/* --- tee logging (debug-reader.c tee pattern, simplified) -------------- */

static FILE* g_log;

static void pf(const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    if(g_log)
        vfprintf(g_log, fmt, ap);
    va_end(ap);

    fputc('\n', stdout);
    if(g_log)
        fputc('\n', g_log);

    fflush(stdout);
    if(g_log)
        fflush(g_log);
}

/* --- per-check state ---------------------------------------------------- */

static int g_failures;

static void check_fail(const char* name, const char* why)
{
    pf("check=%s verdict=FAIL why=%s", name, why);
    g_failures++;
}

/* Classify a raw kekcall result. Returns:
 *   0 = a real VALUE was returned (stored in *value)
 *   1 = error (CF set, or errno-class value, or CHECK-style nonzero).
 * V1: CF is the FIRST and only authoritative error signal; the raw value is
 * never tested for sign. rc == 0 is a valid success (zero MSR value). */
static int classify_msr_result(uint64_t rc, int cf_error, uint64_t* value)
{
    if(cf_error) /* carry-flag error: errno = rc, raw value logged by caller */
        return 1;
    if(rc != 0 && rc < ERRNO_CLASS_MAX) /* errno-in-RAX convention or CHECK-style */
        return 1;
    *value = rc;
    return 0;
}

/* --- main ---------------------------------------------------------------- */

int main(void)
{
    g_log = fopen(PROBE_LOGPATH, "w");
    if(!g_log)
        pf("warn log_open_failed path=" PROBE_LOGPATH " continuing on stdout");

    pf("probe=" PROBE_ID " start");

    /*
     * 1. CHECK — issued exactly ONCE. Refusal law (VERDICT-V2 P1): a failed
     * CHECK is an ABORT, never retried. Polarity (VERIFIED.md G4):
     * rc == 0 = resident AND this process is kekcall-eligible. Any nonzero
     * = the REAL getppid ran (parent pid; kstuff absent, or this payload
     * process is not on the hooked PPR table — open question U1), or an
     * errno. All three are terminal failures here; the raw value and CF are
     * logged so the reconciliation can tell them apart.
     */
    int check_cf = 0;
    const uint64_t check_rc = kekcall(0, 0, 0, 0, 0, 0, KEKCALL_CHECK,
                                      &check_cf);
    if(check_rc == 0)
        pf("check=CHECK rc=0 verdict=PASS meaning=kstuff_resident_and_eligible");
    else if(check_cf)
    {
        pf("evidence=CHECK rc=0x%016" PRIx64 " errno=%" PRIu64
           " convention=cf_set", check_rc, check_rc);
        check_fail("CHECK", "syscall_errno");
    }
    else
    {
        pf("evidence=CHECK rc=0x%016" PRIx64
           " meaning=real_getppid_parent_pid_or_errno kstuff=not_resident_or_process_not_ppr_eligible",
           check_rc);
        check_fail("CHECK", "nonzero_gate_result");
    }

    /* Everything below runs only if CHECK passed; on CHECK failure we still
     * perform NO further kekcalls and exit 2. */
    if(check_rc == 0)
    {
        /*
         * 2. RDMSR LSTAR — the kernel syscall-entry RIP. Known answer is a
         * SHAPE: by architectural definition it must be a canonical
         * kernel-half address (>= 0xffff800000000000, i.e.
         * value >> 47 == 0x1FFFF). Exact bytes logged for independent
         * review. V1: a successful read HAS bit 63 set and now survives
         * classification (v0 bug: sign test ate it).
         */
        int lstar_cf = 0;
        uint64_t lstar = 0;
        const uint64_t lstar_rc = kekcall(MSR_LSTAR, 0, 0, 0, 0, 0,
                                          KEKCALL_RDMSR, &lstar_cf);
        if(classify_msr_result(lstar_rc, lstar_cf, &lstar))
        {
            pf("evidence=RDMSR_LSTAR rc=0x%016" PRIx64 " cf=%d",
               lstar_rc, lstar_cf);
            check_fail("RDMSR_LSTAR", "msr_read_faulted_or_errno");
        }
        else if((lstar >> 47) != UINT64_C(0x1FFFF))
        {
            pf("evidence=RDMSR_LSTAR value=0x%016" PRIx64, lstar);
            check_fail("RDMSR_LSTAR", "not_canonical_kernel_address");
        }
        else
            pf("check=RDMSR_LSTAR value=0x%016" PRIx64 " top32=0x%08" PRIx64
               " canonical_kernel=yes verdict=PASS",
               lstar, lstar >> 32);

        /*
         * 3. RDMSR EFER — logged as evidence only. Expected stable
         * SCE|LME|LMA|NXE (+SVME if nesting on) = 0xD01/0xD11-class, but
         * the exact console value is not pinned, so it does not gate
         * pass/fail.
         */
        int efer_cf = 0;
        uint64_t efer = 0;
        const uint64_t efer_rc = kekcall(MSR_EFER, 0, 0, 0, 0, 0,
                                         KEKCALL_RDMSR, &efer_cf);
        if(classify_msr_result(efer_rc, efer_cf, &efer))
        {
            pf("evidence=RDMSR_EFER rc=0x%016" PRIx64 " cf=%d",
               efer_rc, efer_cf);
            check_fail("RDMSR_EFER", "msr_read_faulted_or_errno");
        }
        else
            pf("evidence=RDMSR_EFER value=0x%016" PRIx64 " verdict=LOGGED_ONLY",
               efer);

        /*
         * 4. RDMSR TSC twice — the counter must advance between two live
         * kernel-context reads. Gates: both reads succeed, strictly
         * increasing, delta logged.
         */
        int t1_cf = 0;
        int t2_cf = 0;
        uint64_t tsc1 = 0;
        uint64_t tsc2 = 0;
        const uint64_t t1_rc = kekcall(MSR_TSC, 0, 0, 0, 0, 0,
                                       KEKCALL_RDMSR, &t1_cf);
        const uint64_t t2_rc = kekcall(MSR_TSC, 0, 0, 0, 0, 0,
                                       KEKCALL_RDMSR, &t2_cf);
        const int t1_bad = classify_msr_result(t1_rc, t1_cf, &tsc1);
        const int t2_bad = classify_msr_result(t2_rc, t2_cf, &tsc2);
        if(t1_bad || t2_bad)
        {
            pf("evidence=RDMSR_TSC t1_rc=0x%016" PRIx64 " t1_cf=%d"
               " t2_rc=0x%016" PRIx64 " t2_cf=%d",
               t1_rc, t1_cf, t2_rc, t2_cf);
            check_fail("RDMSR_TSC", "msr_read_faulted_or_errno");
        }
        else if(tsc2 <= tsc1)
        {
            pf("evidence=RDMSR_TSC t1=0x%016" PRIx64 " t2=0x%016" PRIx64,
               tsc1, tsc2);
            check_fail("RDMSR_TSC", "counter_not_advancing");
        }
        else
            pf("check=RDMSR_TSC t1=0x%016" PRIx64 " t2=0x%016" PRIx64
               " delta=%" PRIu64 " verdict=PASS",
               tsc1, tsc2, tsc2 - tsc1);

        /*
         * 5. READ_DR — 48 bytes: DR0 DR1 DR2 DR3 DR6 DR7 (utils.c:677-690,
         * kekcall.c:23-47). Pre-fill 0xA5; a real kernel->user copyout must
         * change at least the DR6 word (0xFFFF0FF0-class on an idle AMD
         * thread; DR0-3 and DR7 are expected 0). rc must be exactly 0.
         */
        int dr_cf = 0;
        unsigned char dr[48];
        memset(dr, 0xA5, sizeof(dr));
        const uint64_t drrc = kekcall((uint64_t)(uintptr_t)dr, 0, 0,
                                      0, 0, 0, KEKCALL_READ_DR, &dr_cf);
        int moved = 0;
        for(size_t i = 0; i < sizeof(dr); i++)
            if(dr[i] != 0xA5)
                moved = 1;

        if(drrc != 0)
        {
            pf("evidence=READ_DR rc=0x%016" PRIx64 " cf=%d bytes_moved=%d",
               drrc, dr_cf, moved);
            check_fail("READ_DR", "sub1_did_not_return_zero");
        }
        else if(!moved)
            check_fail("READ_DR", "buffer_untouched_no_copyout_evidence");
        else
        {
            pf("check=READ_DR rc=0 verdict=PASS");
            for(size_t i = 0; i < sizeof(dr); i += 8)
            {
                uint64_t v;
                memcpy(&v, dr + i, 8);
                pf("evidence=READ_DR byte_off=%02zu value=0x%016" PRIx64, i, v);
            }

            /* Soft shape checks (logged, non-gating): an idle thread on AMD
             * should show DR0-3 = 0, DR6 0xFFFF0FF0-class with bits 63:32 =
             * 0 (kstuff masks DR6 &= -16 only when the thread owns no
             * dbregs, kekcall.c:32-36), DR7 = 0 or 0x400-class. */
            uint64_t dr0, dr6, dr7;
            memcpy(&dr0, dr + 0, 8);
            memcpy(&dr6, dr + 32, 8);
            memcpy(&dr7, dr + 40, 8);
            pf("evidence=READ_DR shape dr0=0x%016" PRIx64
               " dr6_hi32=0x%08" PRIx64 " dr7=0x%016" PRIx64,
               dr0, dr6 >> 32, dr7);
        }
    }
    else
        pf("note further_checks_skipped reason=check_failed");

    /* 6. Summary */
    pf("summary checks=4 failures=%d", g_failures);
    if(g_failures)
        pf("overall=FAIL exit=%d", check_rc ? 2 : 3);
    else
        pf("overall=PASS exit=0");

    if(g_log)
        fclose(g_log);
    return check_rc ? 2 : (g_failures ? 3 : 0);
}
