/*
 * cap03/src/CodePatch.cpp — Capability #3 implementation.
 *
 * PROPOSAL ONLY (2026-10-13, Mac-only; never built, never run on console).
 * Implements the full protocol from ../DESIGN.md with every return value
 * checked, original-protection capture/restore, and a re-query restoration
 * proof. Deliberate, documented deviations from the gdb_stub/dbg.c:393-448
 * precedent: rc-checked mprotect, restoration, and abort-on-any-failure.
 *
 * Engine facts this builds on (all pinned-source, cited in DESIGN.md):
 *   - kstuff uelf/main.c:87-93: handle_syscall intercepts
 *     IS(mprotect) || IS_PPR(mdbg_call) unconditionally (syscall 74 in
 *     both sysents, mdbg_call 573 on PPR) -> handle_syscall_fix().
 *   - uelf/syscall_fixes.c: dbgregs armed at mprotect_fix_start with
 *     dr7 0x405; the trap rewinds RIP mprotect_fix_start -> mprotect_fix_end,
 *     skipping kernel mprotect's permission-validation block. It does NOT
 *     force the underlying vm_map operation to succeed (U2).
 *   - prosper0gdb/r0gdb.c:962-975 (mprotect20) is the return-checked
 *     precedent; dbg.c is the unchecked, non-restoring one.
 */

#include "CodePatch.hpp"

#include <machine/param.h>   /* PAGE_SIZE, PAGE_MASK (16 KiB under __SCE__) */
#include <sys/mman.h>        /* mprotect, PROT_* (SDK sys/mman.h:52-55,263) */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace cap03 {

/* ================================================================== */
/* Locally declared SCE APIs                                           */
/* ================================================================== */

/* Neither function is declared by the pinned ps5-payload-sdk v0.43
 * headers; both symbols ARE exported by the pinned libkernel.so
 * (nm -D --defined-only verified: sceKernelVirtualQuery,
 *  sceKernelQueryMemoryProtection). Signatures are the public PS4-family
 * ones. Their exact PS5 semantics are U3 until console-verified. */

extern "C" int sceKernelQueryMemoryProtection(const void* addr,
                                              void** start, void** end,
                                              int* prot);

/* PS4-derived layout, PUBLIC-UNVERIFIED for PS5 (U3). The code never
 * trusts fields blindly: query_span_protection_vq() requires the reported
 * start/end to bracket the queried address before reporting kOk. */
struct VqInfo {
    void*    start;    /* 0x00 */
    void*    end;      /* 0x08 */
    uint64_t offset;   /* 0x10 (PS4: uint32; PS5 believed widened) */
    int32_t  prot;     /* 0x18 (PS4: 0x14) — UNVERIFIED             */
    int32_t  flags;    /* 0x1c                                      */
    char     name[32]; /* 0x20                                      */
};                     /* size believed 0x40                        */

extern "C" int sceKernelVirtualQuery(const void* addr, int class_idx,
                                     VqInfo* info, size_t info_size);

/* ================================================================== */
/* Logging                                                             */
/* ================================================================== */

static PatchLogFn g_log_fn = 0;
static void*      g_log_user = 0;
static uint32_t   g_log_seq = 0;

/* Slot consumed by unpatch_code(). Published only by a successful
 * patch_code_ex. Declared here so patch_code_ex can publish it. */
static PatchBackup g_last_backup;
static bool        g_last_backup_seen;

static const int kHexCap = 16;

void set_patch_log_sink(PatchLogFn fn, void* user)
{
    g_log_fn = fn;
    g_log_user = user;
}

static void emit_line(const char* line)
{
    ++g_log_seq;
    if(g_log_fn)
        g_log_fn(line, g_log_user);
    else
        printf("%06u %s\n", (unsigned)g_log_seq, line);
}

/* printf-style step logger with sequence numbers (contiguous log contract,
 * verdict section 3b step 8). */
static void emitf(const char* fmt, ...)
{
    char buf[384];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    emit_line(buf);
}

void log_hex_bytes(const char* tag, const void* addr, size_t n)
{
    if(n > (size_t)kHexCap)
        n = (size_t)kHexCap;
    char hex[kHexCap * 3 + 1];
    const uint8_t* p = (const uint8_t*)addr;
    size_t off = 0;
    for(size_t i = 0; i < n; ++i)
        off += (size_t)snprintf(hex + off, sizeof(hex) - off, "%02x ", p[i]);
    if(off)
        hex[off - 1] = 0; /* trim trailing space */
    else
        hex[0] = 0;
    emitf("[CAP3][HEX] %s @%p len=%zu %s%s", tag, addr,
          n > (size_t)kHexCap ? (size_t)kHexCap : n, hex,
          n > (size_t)kHexCap ? "(capped)" : "");
}

void prot_decode(int prot, char out[8])
{
    /* R=1 W=2 X=4 assumed to match FreeBSD PROT_* (sys/mman.h:52-55).
     * Unknown bits (incl. 0x8) surface as '?' instead of being silently
     * reinterpreted — U3 honesty requirement. */
    int known = 0;
    char* o = out;
    if(prot & PROT_READ)  { *o++ = 'R'; known |= PROT_READ; }
    if(prot & PROT_WRITE) { *o++ = 'W'; known |= PROT_WRITE; }
    if(prot & PROT_EXEC)  { *o++ = 'X'; known |= PROT_EXEC; }
    if(prot == 0)          { *o++ = '-'; known = 0; }
    if((unsigned)prot & ~(unsigned)known) *o++ = '?';
    *o = 0;
}

const char* patch_result_str(PatchResult r)
{
    switch(r)
    {
    case PatchResult::kOk:                  return "OK";
    case PatchResult::kBadArgs:             return "BAD_ARGS";
    case PatchResult::kBadFlags:            return "BAD_FLAGS";
    case PatchResult::kTooLarge:            return "TOO_LARGE";
    case PatchResult::kMixedProtection:     return "MIXED_PROTECTION";
    case PatchResult::kBackupInvalid:       return "BACKUP_INVALID";
    case PatchResult::kNotPatchedState:     return "NOT_PATCHED_STATE";
    case PatchResult::kQueryFailed:         return "QUERY_FAILED";
    case PatchResult::kLivenessFailed:      return "LIVENESS_FAILED";
    case PatchResult::kMprotectFailed:      return "MPROTECT_FAILED";
    case PatchResult::kVerifyOpenFailed:    return "VERIFY_OPEN_FAILED";
    case PatchResult::kReadbackMismatch:    return "READBACK_MISMATCH";
    case PatchResult::kAlreadyApplied:      return "ALREADY_APPLIED";
    case PatchResult::kOriginalMismatch:    return "ORIGINAL_MISMATCH";
    case PatchResult::kRestoreWriteFailed:  return "RESTORE_WRITE_FAILED";
    case PatchResult::kRestoreMprotectFailed: return "RESTORE_MPROTECT_FAILED";
    case PatchResult::kRestoreVerifyFailed: return "RESTORE_VERIFY_FAILED";
    case PatchResult::kRestoreQueryFailed:  return "RESTORE_QUERY_FAILED";
    }
    return "UNKNOWN";
}

/* ================================================================== */
/* Query primitives                                                    */
/* ================================================================== */

PatchResult query_span_protection(const void* addr, SpanProtection* out)
{
    if(!addr || !out)
        return PatchResult::kBadArgs;
    void* start = 0;
    void* end = 0;
    int prot = 0;
    errno = 0;
    int rc = sceKernelQueryMemoryProtection(addr, &start, &end, &prot);
    out->start = start;
    out->end = end;
    out->prot = prot;
    char dec[8];
    prot_decode(prot, dec);
    const uintptr_t queried = (uintptr_t)addr;
    const bool bracketed = start && end &&
        (uintptr_t)start <= queried && queried < (uintptr_t)end;
    emitf("[CAP3][QUERY] qmp addr=%p rc=%d errno=%d prot=0x%x(%s) span=%p..%p bracketed=%d",
          addr, rc, errno, (unsigned)prot, dec, start, end, (int)bracketed);
    if(rc != 0 || !bracketed)
        return PatchResult::kQueryFailed;
    return PatchResult::kOk;
}

PatchResult query_span_protection_vq(const void* addr, SpanProtection* out)
{
    if(!addr || !out)
        return PatchResult::kBadArgs;
    VqInfo info;
    memset(&info, 0, sizeof(info));
    errno = 0;
    int rc = sceKernelVirtualQuery(addr, 0, &info, sizeof(info));
    out->start = info.start;
    out->end = info.end;
    out->prot = info.prot;
    char dec[8];
    prot_decode(info.prot, dec);
    /* Layout sanity: a wrong PS5 struct layout must SHOW here, not pass
     * garbage onward (U3 detection). */
    int bracketed = info.start && info.end &&
        (uintptr_t)info.start <= (uintptr_t)addr &&
        (uintptr_t)addr < (uintptr_t)info.end;
    emitf("[CAP3][QUERY] vq addr=%p rc=%d errno=%d prot=0x%x(%s) span=%p..%p "
          "name='%.32s' bracketed=%d",
          addr, rc, errno, (unsigned)info.prot, dec,
          info.start, info.end, info.name, bracketed);
    if(rc != 0 || !bracketed)
        return PatchResult::kQueryFailed;
    return PatchResult::kOk;
}

/* kekcall CHECK — inline asm transcribed from the pinned
 * ps5-kstuff/debug-reader.c:23-46 wrapper: six register arguments, the
 * seventh (sub-op) rides RAX into `syscall`. Sub-op
 * 0xffffffff00000027 = kekcall prefix 0xffffffff | getppid 39
 * (debug-reader.c:9-15; handler uelf/kekcall.c:102-106 returns 0).
 * If the process does not land on the hooked PPR getppid entry the raw
 * getppid executes and returns a pid (non-zero) — fail-safe (U1). */
#define KEKCALL_CHECK 0xffffffff00000027ull

int kekcall_check(void)
{
    int64_t ret;
    unsigned char is_error;
    register uint64_t r10 __asm__("r10") = 0;
    register uint64_t r8  __asm__("r8")  = 0;
    register uint64_t r9  __asm__("r9")  = 0;
    __asm__ __volatile__(
        "syscall"
        : "=a"(ret), "=@ccc"(is_error), "+r"(r10), "+r"(r8), "+r"(r9)
        : "a"(KEKCALL_CHECK), "D"(0), "S"(0), "d"(0)
        : "rcx", "r11", "memory");
    int64_t value = is_error ? -(int64_t)(uint64_t)ret : (int64_t)(uint64_t)ret;
    emitf("[CAP3][LIVENESS] kekcall_check subop=0x%llx rc=%lld errno=%d -> %s",
          (unsigned long long)KEKCALL_CHECK, (long long)value, errno,
          value == 0 ? "ALIVE" : "NOT-ALIVE");
    return value == 0 ? 0 : 1;
}

/* ================================================================== */
/* Span bookkeeping                                                    */
/* ================================================================== */

struct Span {
    void*  base;
    size_t len;         /* page-aligned                       */
    int    page_count;  /* 1..kMaxPatchPages                  */
    int    orig_prot[kMaxPatchPages];
};

static void span_of(void* target, size_t length, Span* span)
{
    uintptr_t first = (uintptr_t)target & ~(uintptr_t)PAGE_MASK;
    uintptr_t last = ((uintptr_t)target + length - 1) & ~(uintptr_t)PAGE_MASK;
    span->base = (void*)first;
    span->len = (last - first) + PAGE_SIZE;
    span->page_count = (int)(span->len / PAGE_SIZE);
    for(int i = 0; i < kMaxPatchPages; ++i)
        span->orig_prot[i] = -1;
}

static bool span_address_valid(const void* target, size_t length)
{
    return target && length > 0 && length <= kMaxPatchBytes &&
        (uintptr_t)target <= UINTPTR_MAX - (length - 1);
}

/* ================================================================== */
/* Protocol helpers                                                    */
/* ================================================================== */

/* Query every page of the span; require identical raw protection on all
 * pages (single restore value, verdict protocol step 7b uses one
 * original_protection). Queried protections are written to queried_prot,
 * a caller-supplied array.
 *
 * HOST-TEST FIX (cap03 KD, 2026-10-13): this helper used to record into
 * span->orig_prot directly, and unpatch_code_ex() called it to observe
 * the current state — silently OVERWRITING the backup's original
 * protections with the current (open, RWX) ones. unpatch then "restored"
 * RWX and the re-query proof compared against the wrong value. Mac host
 * test tests/host_test_protocol.cpp caught it; the unpatch path now
 * queries into scratch storage and keeps the backup's restore values. */
static PatchResult preflight_query_pages(Span* span,
                                         int queried_prot[kMaxPatchPages])
{
    for(int i = 0; i < kMaxPatchPages; ++i)
        queried_prot[i] = -1;
    for(int i = 0; i < span->page_count; ++i)
    {
        const void* page = (const void*)((uintptr_t)span->base
                                         + (size_t)i * PAGE_SIZE);
        SpanProtection sp;
        PatchResult r = query_span_protection(page, &sp);
        if(r != PatchResult::kOk)
            return PatchResult::kQueryFailed;
        /* The pre-flight byte capture below must never touch execute-only
         * memory. A future kernel-copy provider may enable that case, but
         * this in-process implementation cannot safely read it. */
        if((sp.prot & PROT_READ) == 0)
        {
            emitf("[CAP3][PATCH] STEP=PRE-FLIGHT page=%d prot=0x%x "
                  "is not readable -> refuse before byte access", i,
                  (unsigned)sp.prot);
            return PatchResult::kQueryFailed;
        }
        /* Corroborate with VirtualQuery (U3 fidelity measurement).
         * Failure here is logged but NOT fatal: qmp stays primary. */
        SpanProtection vsp;
        (void)query_span_protection_vq(page, &vsp);
        queried_prot[i] = sp.prot;
    }
    /* Single-protection requirement. */
    for(int i = 1; i < span->page_count; ++i)
    {
        if(queried_prot[i] != queried_prot[0])
        {
            emitf("[CAP3][PATCH] STEP=PRE-FLIGHT rc=0 orig_prot[0]=0x%x "
                  "orig_prot[%d]=0x%x -> MIXED (refuse)",
                  (unsigned)queried_prot[0], i,
                  (unsigned)queried_prot[i]);
            return PatchResult::kMixedProtection;
        }
    }
    return PatchResult::kOk;
}

/* VERIFY-OPEN: every page of the span must now report writable (and
 * readable). Primary = qmp. VirtualQuery disagreement on writability is
 * treated as failure (conservative; U3 fidelity is being tested, never
 * assumed). */
static PatchResult verify_open_pages(const Span* span)
{
    for(int i = 0; i < span->page_count; ++i)
    {
        const void* page = (const void*)((uintptr_t)span->base
                                         + (size_t)i * PAGE_SIZE);
        SpanProtection sp;
        if(query_span_protection(page, &sp) != PatchResult::kOk)
            return PatchResult::kVerifyOpenFailed;
        SpanProtection vsp;
        PatchResult vr = query_span_protection_vq(page, &vsp);
        bool writable = ((sp.prot & PROT_WRITE) != 0)
                     && ((sp.prot & PROT_READ) != 0);
        bool vq_disagrees = (vr == PatchResult::kOk)
                         && ((vsp.prot & PROT_WRITE) == 0);
        if(!writable || vq_disagrees)
        {
            emitf("[CAP3][PATCH] STEP=VERIFY-OPEN page=%d prot=0x%x "
                  "writable=%d vq_disagrees=%d -> NOT OPEN",
                  i, (unsigned)sp.prot, (int)writable, (int)vq_disagrees);
            return PatchResult::kVerifyOpenFailed;
        }
    }
    return PatchResult::kOk;
}

/* WRITE + readback. Returns kOk only if the byte-compare passes. */
static PatchResult write_and_verify(void* target, const void* bytes,
                                    size_t length, const char* what)
{
    memcpy(target, bytes, length);
    /* Compiler + CPU barrier. x86-64 stores are coherent for this core;
     * cross-core visibility to EXECUTING cores is U4 — the caller must
     * guarantee no other thread executes the span until after this
     * readback passes (DESIGN.md step 6). */
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    __asm__ __volatile__("" : : : "memory");

    if(memcmp(target, bytes, length) != 0)
    {
        emitf("[CAP3][WRITE] STEP=READBACK target=%p len=%zu (%s) -> MISMATCH",
              target, length, what);
        log_hex_bytes("readback", target, length);
        return PatchResult::kReadbackMismatch;
    }
    emitf("[CAP3][WRITE] STEP=WRITE+READBACK target=%p len=%zu (%s) rc=0",
          target, length, what);
    log_hex_bytes("written", target, length);
    return PatchResult::kOk;
}

/* REVERT-ONLY path: put the span back exactly as pre-flight found it.
 * VERIFY-OPEN failures have not written any bytes, and may mean the page is
 * still read-only; those failures restore permissions without a byte write.
 * A restoration failure takes priority over the original error so callers
 * can distinguish a clean refusal from a DIRTY state. */
static PatchResult revert_only(void* target, const void* original_bytes,
                               size_t length, const Span* span,
                               const char* ctx, bool restore_bytes)
{
    emitf("[CAP3][REVERT] %s: reverting target=%p len=%zu span=%p+0x%zx "
          "restore_bytes=%d", ctx, target, length, span->base, span->len,
          (int)restore_bytes);

    PatchResult result = PatchResult::kOk;
    if(restore_bytes)
    {
        PatchResult rw = write_and_verify(target, original_bytes, length,
                                          "revert-bytes");
        if(rw != PatchResult::kOk)
        {
            emitf("[CAP3][REVERT] RESTORE-BYTES-FAILED rc=%d (%s) — STATE DIRTY",
                  (int)rw, patch_result_str(rw));
            result = PatchResult::kRestoreWriteFailed;
        }
    }

    /* R2: original protection, per page, rc checked (verdict step 7b). */
    for(int i = 0; i < span->page_count; ++i)
    {
        void* page = (void*)((uintptr_t)span->base + (size_t)i * PAGE_SIZE);
        errno = 0;
        int rc = mprotect(page, PAGE_SIZE, span->orig_prot[i]);
        emitf("[CAP3][REVERT] STEP=RESTORE-PROT page=%d %p prot=0x%x rc=%d "
              "errno=%d", i, page, (unsigned)span->orig_prot[i], rc, errno);
        if(rc != 0)
        {
            emitf("[CAP3][REVERT] RESTORE-MPROTECT-FAILED page=%d — STATE DIRTY",
                  i);
            if(result == PatchResult::kOk)
                result = PatchResult::kRestoreMprotectFailed;
        }
    }

    /* R3: restoration proof — re-query must equal the recorded original
     * (verdict step 7c: the proof dbg.c never supplies). */
    for(int i = 0; i < span->page_count; ++i)
    {
        const void* page = (const void*)((uintptr_t)span->base
                                         + (size_t)i * PAGE_SIZE);
        SpanProtection sp;
        if(query_span_protection(page, &sp) != PatchResult::kOk)
        {
            emitf("[CAP3][REVERT] RESTORE-VERIFY-UNREADABLE page=%d — STATE DIRTY",
                  i);
            if(result == PatchResult::kOk)
                result = PatchResult::kRestoreQueryFailed;
            continue;
        }
        if(sp.prot != span->orig_prot[i])
        {
            emitf("[CAP3][REVERT] RESTORE-VERIFY-FAILED page=%d have=0x%x "
                  "want=0x%x — STATE DIRTY", i,
                  (unsigned)sp.prot, (unsigned)span->orig_prot[i]);
            if(result == PatchResult::kOk)
                result = PatchResult::kRestoreVerifyFailed;
        }
        else
            emitf("[CAP3][REVERT] RESTORE-PROOF page=%d prot=0x%x == original",
                  i, (unsigned)sp.prot);
    }
    return result;
}

/* ================================================================== */
/* patch_code_ex                                                       */
/* ================================================================== */

static PatchResult patch_code_impl(void* target, const void* replacement,
                                   const void* expected_original,
                                   bool require_expected, size_t length,
                                   PatchBackup* backup_out, unsigned flags)
{
    emitf("[CAP3][PATCH] === BEGIN target=%p len=%zu flags=0x%x ===",
          target, length, flags);

    /* STEP 0: arguments. */
    if(!target || !replacement || (require_expected && !expected_original) ||
       length == 0 ||
       (length <= kMaxPatchBytes && !span_address_valid(target, length)))
        return PatchResult::kBadArgs;
    if(length > kMaxPatchBytes)
        return PatchResult::kTooLarge;
    if((flags & kFlagRequireLiveness) && (flags & kFlagSkipLiveness))
        return PatchResult::kBadFlags;

    Span span;
    span_of(target, length, &span);
    if(span.page_count > kMaxPatchPages)
        return PatchResult::kTooLarge;

    /* STEP 1-2 (P2, P3): PRE-FLIGHT — record mapping + protection, log
     * first bytes, capture originals. Nothing modified yet. The queried
     * protections ARE the restore values on this path. */
    PatchResult pre = preflight_query_pages(&span, span.orig_prot);
    if(pre != PatchResult::kOk)
        return pre;
    log_hex_bytes("original", target, length);

    uint8_t expected_copy[kMaxPatchBytes];
    if(require_expected)
        memcpy(expected_copy, expected_original, length);

    PatchBackup local_backup;
    memset(&local_backup, 0, sizeof(local_backup));
    local_backup.magic = kBackupMagic;
    local_backup.page_count = (uint32_t)span.page_count;
    local_backup.target = target;
    local_backup.length = length;
    local_backup.page_base = span.base;
    local_backup.span_len = span.len;
    for(int i = 0; i < span.page_count; ++i)
        local_backup.orig_prot[i] = span.orig_prot[i];
    memcpy(local_backup.original_bytes, target, length);
    memcpy(local_backup.replacement_bytes, replacement, length);
    local_backup.valid = 1;

    if(require_expected &&
       memcmp(local_backup.original_bytes, expected_copy, length) != 0)
    {
        emitf("[CAP3][PATCH] STEP=COMPARE original != planned snapshot "
              "before OPEN -> ORIGINAL_MISMATCH (nothing written)");
        return PatchResult::kOriginalMismatch;
    }

    /* STEP 2b (P1): LIVENESS — kekcall CHECK must report alive before the
     * page is opened (verdict precondition P1: a failed probe aborts, it
     * is never retried blindly). */
    if(flags & kFlagRequireLiveness)
    {
        errno = 0;
        if(kekcall_check() != 0)
            return PatchResult::kLivenessFailed;
    }

    /* Compare-before-write: refuse an idempotent-looking double patch. */
    if(memcmp(target, local_backup.replacement_bytes, length) == 0)
    {
        emitf("[CAP3][PATCH] STEP=COMPARE target already == replacement -> "
              "ALREADY_APPLIED (nothing written)");
        return PatchResult::kAlreadyApplied;
    }

    /* STEP 3: OPEN — mprotect RWX, RETURN VALUE CHECKED (the deviation
     * from dbg.c:393-396 is deliberate and documented). */
    errno = 0;
    int rc = mprotect(span.base, span.len,
                      PROT_READ | PROT_WRITE | PROT_EXEC);
    emitf("[CAP3][PATCH] STEP=OPEN mprotect(%p, 0x%zx, RWX) rc=%d errno=%d",
          span.base, span.len, rc, errno);
    if(rc != 0)
        return PatchResult::kMprotectFailed; /* nothing changed yet */

    /* STEP 4: VERIFY-OPEN — re-query; on failure go REVERT-ONLY
     * ("never write on an unverified open", verdict U3 handling). */
    if(verify_open_pages(&span) != PatchResult::kOk)
    {
        emitf("[CAP3][PATCH] VERIFY-OPEN failed -> REVERT-ONLY");
        PatchResult rr = revert_only(target, local_backup.original_bytes,
                                     length, &span, "verify-open", false);
        return rr == PatchResult::kOk ? PatchResult::kVerifyOpenFailed : rr;
    }

    if(require_expected && memcmp(target, expected_copy, length) != 0)
    {
        emitf("[CAP3][PATCH] STEP=COMPARE original changed after OPEN -> "
              "ORIGINAL_MISMATCH (no replacement written)");
        PatchResult rr = revert_only(target, local_backup.original_bytes,
                                     length, &span, "original-mismatch", false);
        return rr == PatchResult::kOk ? PatchResult::kOriginalMismatch : rr;
    }

    /* STEP 5: WRITE + readback. Any mismatch -> REVERT-ONLY. */
    if(write_and_verify(target, local_backup.replacement_bytes, length,
                        "patch") !=
       PatchResult::kOk)
    {
        emitf("[CAP3][PATCH] WRITE failed -> REVERT-ONLY");
        PatchResult rr = revert_only(target, local_backup.original_bytes,
                                     length, &span, "write", true);
        return rr == PatchResult::kOk ? PatchResult::kReadbackMismatch : rr;
    }

    /* STEP 6: execution barrier is a CALLER duty (DESIGN.md: hold other
     * threads off the span until the readback above passed). Logged, not
     * enforceable in-process without thread introspection (U4). */
    emitf("[CAP3][PATCH] STEP=BARRIER caller must guarantee the span was "
          "quiescent during the patch (U4)");

    if(backup_out)
        *backup_out = local_backup;
    /* Publish to the module slot consumed by unpatch_code(). */
    g_last_backup = local_backup;

    emitf("[CAP3][PATCH] === PASS target=%p len=%zu orig_prot=0x%x ===",
          target, length, (unsigned)span.orig_prot[0]);
    return PatchResult::kOk;
}

PatchResult patch_code_ex(void* target, const void* replacement,
                          size_t length, PatchBackup* backup_out,
                          unsigned flags)
{
    return patch_code_impl(target, replacement, 0, false, length,
                           backup_out, flags);
}

PatchResult patch_code_if_original_ex(void* target, const void* replacement,
                                      const void* expected_original,
                                      size_t length, PatchBackup* backup_out,
                                      unsigned flags)
{
    return patch_code_impl(target, replacement, expected_original, true,
                           length, backup_out, flags);
}

/* ================================================================== */
/* unpatch_code_ex                                                     */
/* ================================================================== */

PatchResult unpatch_code_ex(PatchBackup* backup, unsigned flags)
{
    if(!backup || backup->magic != kBackupMagic || !backup->valid)
        return PatchResult::kBackupInvalid;
    if((flags & kFlagRequireLiveness) && (flags & kFlagSkipLiveness))
        return PatchResult::kBadFlags;

    void* target = backup->target;
    size_t length = backup->length;
    if(!span_address_valid(target, length) ||
       backup->page_count == 0 || backup->page_count > kMaxPatchPages)
        return PatchResult::kBackupInvalid;
    Span expected;
    span_of(target, length, &expected);
    if((uint32_t)expected.page_count != backup->page_count ||
       expected.base != backup->page_base || expected.len != backup->span_len)
        return PatchResult::kBackupInvalid;
    emitf("[CAP3][UNPATCH] === BEGIN target=%p len=%zu flags=0x%x ===",
          target, length, flags);

    Span span;
    span.base = backup->page_base;
    span.len = backup->span_len;
    span.page_count = (int)backup->page_count;
    for(int i = 0; i < kMaxPatchPages; ++i)
        span.orig_prot[i] = backup->orig_prot[i];

    /* PRE: record current state + compare-before-write against the
     * recorded replacement (target must be in the patched state). The
     * queried protections go to SCRATCH: the restore values stay the
     * backup's originals (host-test fix, see preflight_query_pages). */
    int cur_prot[kMaxPatchPages];
    PatchResult pre = preflight_query_pages(&span, cur_prot);
    if(pre != PatchResult::kOk)
        return pre;
    if(memcmp(target, backup->replacement_bytes, length) != 0)
    {
        emitf("[CAP3][UNPATCH] target bytes != recorded replacement -> "
              "NOT_PATCHED_STATE (nothing written)");
        log_hex_bytes("current", target, length);
        return PatchResult::kNotPatchedState;
    }

    if(flags & kFlagRequireLiveness)
    {
        errno = 0;
        if(kekcall_check() != 0)
            return PatchResult::kLivenessFailed;
    }

    /* OPEN (rc checked). */
    errno = 0;
    int rc = mprotect(span.base, span.len,
                      PROT_READ | PROT_WRITE | PROT_EXEC);
    emitf("[CAP3][UNPATCH] STEP=OPEN mprotect(%p, 0x%zx, RWX) rc=%d errno=%d",
          span.base, span.len, rc, errno);
    if(rc != 0)
        return PatchResult::kMprotectFailed;

    if(verify_open_pages(&span) != PatchResult::kOk)
    {
        emitf("[CAP3][UNPATCH] VERIFY-OPEN failed -> REVERT-ONLY");
        PatchResult rr = revert_only(target, backup->replacement_bytes,
                                     length, &span, "unpatch-verify-open",
                                     false);
        return rr == PatchResult::kOk ? PatchResult::kVerifyOpenFailed : rr;
    }

    if(write_and_verify(target, backup->original_bytes, length,
                        "unpatch") != PatchResult::kOk)
    {
        emitf("[CAP3][UNPATCH] WRITE failed -> REVERT-ONLY");
        PatchResult rr = revert_only(target, backup->replacement_bytes,
                                     length, &span, "unpatch-write", true);
        return rr == PatchResult::kOk ? PatchResult::kReadbackMismatch : rr;
    }

    /* RESTORE: original protection, rc checked. Retry budget (verdict
     * 7c): exactly ONE retry, granted ONLY for a final verification
     * mismatch. A failed mprotect rc or a failed query is terminal —
     * never blind-retry an ambiguous write. */
    PatchResult restore = PatchResult::kOk;
    for(int attempt = 0; attempt < 2; ++attempt)
    {
        /* HOST-TEST FIX (cap03 KD, 2026-10-13): each attempt starts with
         * a clean restore result. Previously a failed attempt left
         * restore == kRestoreVerifyFailed, and the `restore != kOk`
         * terminal check broke out of the retry even when the retry had
         * PROVEN restoration (re-query == original) — the verdict-7c
         * single retry could never succeed. */
        restore = PatchResult::kOk;
        bool rc_failed = false;
        for(int i = 0; i < span.page_count; ++i)
        {
            void* page = (void*)((uintptr_t)span.base
                                 + (size_t)i * PAGE_SIZE);
            errno = 0;
            int prc = mprotect(page, PAGE_SIZE, span.orig_prot[i]);
            emitf("[CAP3][UNPATCH] STEP=RESTORE-PROT page=%d %p prot=0x%x "
                  "rc=%d errno=%d attempt=%d",
                  i, page, (unsigned)span.orig_prot[i], prc, errno, attempt);
            if(prc != 0)
            {
                rc_failed = true;
                break;
            }
        }
        if(rc_failed)
        {
            restore = PatchResult::kRestoreMprotectFailed;
            break; /* terminal: rc failure gets no retry */
        }

        bool mismatch = false;
        for(int i = 0; i < span.page_count; ++i)
        {
            const void* page = (const void*)
                ((uintptr_t)span.base + (size_t)i * PAGE_SIZE);
            SpanProtection sp;
            if(query_span_protection(page, &sp) != PatchResult::kOk)
            {
                restore = PatchResult::kRestoreQueryFailed;
                mismatch = false;
                break; /* terminal: query ambiguity gets no retry */
            }
            if(sp.prot != span.orig_prot[i])
            {
                emitf("[CAP3][UNPATCH] RESTORE-VERIFY-MISMATCH page=%d "
                      "have=0x%x want=0x%x", i,
                      (unsigned)sp.prot, (unsigned)span.orig_prot[i]);
                mismatch = true;
                break;
            }
        }
        if(restore != PatchResult::kOk)
            break; /* terminal query failure */
        if(!mismatch)
        {
            backup->valid = 0;
            emitf("[CAP3][UNPATCH] === PASS restoration proven "
                  "orig_prot=0x%x ===", (unsigned)span.orig_prot[0]);
            return PatchResult::kOk;
        }
        restore = PatchResult::kRestoreVerifyFailed;
        if(attempt == 0)
            emitf("[CAP3][UNPATCH] restore retry 1/1 (verdict 7c single "
                  "retry, mismatch only; never more)");
    }

    emitf("[CAP3][UNPATCH] RESTORE-FAILED rc=%d (%s) — STATE DIRTY, refusing "
          "further patches this boot",
          (int)restore, patch_result_str(restore));
    return restore;
}

/* ================================================================== */
/* Module-slot entries (task-required signatures)                      */
/* ================================================================== */

PatchResult patch_code(void* target, const void* replacement,
                       size_t length)
{
    g_last_backup_seen = false;
    PatchResult r = patch_code_ex(target, replacement, length,
                                  &g_last_backup, kFlagRequireLiveness);
    g_last_backup_seen = (r == PatchResult::kOk);
    return r;
}

PatchResult unpatch_code(void* target, const void* original,
                         size_t length)
{
    if(!target || !original || length == 0)
        return PatchResult::kBadArgs;
    if(!g_last_backup_seen || g_last_backup.magic != kBackupMagic
       || !g_last_backup.valid)
    {
        emitf("[CAP3][UNPATCH] no valid backup from patch_code() — refusing "
              "(never guess the original protection)");
        return PatchResult::kBackupInvalid;
    }
    if(g_last_backup.target != target || g_last_backup.length != length)
    {
        emitf("[CAP3][UNPATCH] target/length != last patch_code() — refusing");
        return PatchResult::kBackupInvalid;
    }
    /* Caller-supplied bytes are compare-checked against the module's own
     * pre-OPEN capture (P3). Divergence = refuse, never write. */
    if(memcmp(original, g_last_backup.original_bytes, length) != 0)
    {
        emitf("[CAP3][UNPATCH] caller 'original' diverges from pre-flight "
              "capture — refusing");
        return PatchResult::kBadArgs;
    }
    return unpatch_code_ex(&g_last_backup, kFlagRequireLiveness);
}

/* ================================================================== */
/* PatchScope                                                          */
/* ================================================================== */

PatchScope::PatchScope(void* target, const void* replacement,
                       size_t length, unsigned flags)
    : status_(PatchResult::kBadArgs)
    , armed_(false)
    , revert_attempted_(false)
{
    memset(&backup_, 0, sizeof(backup_));
    status_ = patch_code_ex(target, replacement, length, &backup_, flags);
    armed_ = (status_ == PatchResult::kOk);
}

PatchScope::~PatchScope()
{
    if(armed_)
    {
        if(!revert_attempted_)
        {
            /* Normal path: destructor performs the first (and only)
             * restore cycle. */
            PatchResult r = unpatch_code_ex(&backup_, 0);
            if(r != PatchResult::kOk)
                emitf("[CAP3][SCOPE-LEAK] destructor revert failed rc=%d (%s) "
                      "— STATE DIRTY", (int)r, patch_result_str(r));
        }
        else
        {
            /* revert() already ran and failed: the protocol's single
             * retry budget (verdict 7c) is spent. Log loudly; never
             * blind-retry past the bound. */
            emitf("[CAP3][SCOPE-LEAK] revert() already failed (rc=%d %s); "
                  "destructor will NOT retry past the protocol bound — "
                  "STATE DIRTY",
                  (int)status_, patch_result_str(status_));
        }
        armed_ = false;
    }
}

bool PatchScope::ok() const
{
    return armed_;
}

PatchResult PatchScope::status() const
{
    return status_;
}

PatchResult PatchScope::revert()
{
    if(!armed_)
        return status_;
    revert_attempted_ = true;
    status_ = unpatch_code_ex(&backup_, 0);
    armed_ = (status_ != PatchResult::kOk);
    return status_;
}

} /* namespace cap03 */
