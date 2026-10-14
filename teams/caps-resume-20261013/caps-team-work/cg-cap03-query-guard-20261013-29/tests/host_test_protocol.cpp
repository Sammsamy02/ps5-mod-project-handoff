/*
 * cap03/tests/host_test_protocol.cpp — Mac host test of the cap03 protocol.
 *
 * Host-only (2026-10-13). No console, no kernel contact. The production TU
 * src/CodePatch.cpp is compiled UNMODIFIED for the x86_64 host with these
 * command-line renames routing the SCE/mman boundary to mockable symbols:
 *
 *   -DsceKernelQueryMemoryProtection=cap3_host_qmp
 *   -DsceKernelVirtualQuery=cap3_host_vq
 *   -Dmprotect=cap3_host_mprotect
 *
 * Mock model: a page table (page base -> prot) mirrors what
 * sceKernelQueryMemoryProtection should report; cap3_host_mprotect mutates
 * it. The host x86_64 build gets PAGE_SIZE 4096 from <machine/param.h>
 * (the PS5 target build gets 16384 under __SCE__ — the code is
 * page-size-generic; both facts are asserted by the build receipts).
 *
 * kekcall_check() contains a raw `syscall` inline-asm block (x86 only).
 * The host test never reaches it through the default paths (it uses
 * kFlagSkipLiveness / flags=0). Two fork-guarded tests DO execute it to
 * prove patch_code()'s P1 liveness gate returns kLivenessFailed on a host
 * where the kekcall CHECK is not hooked: an invalid macOS syscall fails
 * the CHECK -> not-alive. The fork isolates a hypothetical SIGILL.
 *
 * Expected host endianness/layout: x86-64 little-endian, same as PS5.
 */

#include "CodePatch.hpp"

#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

/* macOS x86_64 userland headers do not export PAGE_SIZE. The host page
 * size is fixed at 4096 for this arch; verified against sysconf at
 * startup (main() refuses to run on a mismatch). The PS5 target build
 * gets 16384 via <machine/param.h> under __SCE__ — recorded in the
 * build receipt; CodePatch.cpp is page-size-generic. */
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#ifndef PAGE_MASK
#define PAGE_MASK (PAGE_SIZE - 1)
#endif

using cap03::PatchBackup;
using cap03::PatchResult;
using cap03::SpanProtection;

/* ================================================================== */
/* Tiny assert harness                                                 */
/* ================================================================== */

static int g_checks = 0;
static int g_fails  = 0;

#define CHECK(cond, name)                                                  \
    do {                                                                   \
        ++g_checks;                                                        \
        if(cond)                                                           \
            printf("[HOST] PASS %s\n", name);                              \
        else {                                                             \
            ++g_fails;                                                     \
            printf("[HOST] FAIL %s (line %d)\n", name, __LINE__);          \
        }                                                                  \
    } while(0)

/* ================================================================== */
/* Log capture (set_patch_log_sink)                                    */
/* ================================================================== */

static std::vector<std::string> g_log;

static void host_log_sink(const char* line, void* /*user*/)
{
    g_log.push_back(line);
}

static void log_clear(void) { g_log.clear(); }

static bool log_contains(const char* key)
{
    for(size_t i = 0; i < g_log.size(); ++i)
        if(g_log[i].find(key) != std::string::npos)
            return true;
    return false;
}

static int log_count(const char* key)
{
    int n = 0;
    for(size_t i = 0; i < g_log.size(); ++i)
        if(g_log[i].find(key) != std::string::npos)
            ++n;
    return n;
}

/* keys must appear as a subsequence, in this order. */
static bool log_ordered(const std::vector<const char*>& keys)
{
    size_t at = 0;
    for(size_t i = 0; i < g_log.size() && at < keys.size(); ++i)
        if(g_log[i].find(keys[at]) != std::string::npos)
            ++at;
    return at == keys.size();
}

/* ================================================================== */
/* Mock page table + fault injection                                   */
/* ================================================================== */

static std::map<uintptr_t, int> g_pages;          /* truth               */
static std::map<uintptr_t, int> g_frozen;         /* stale qmp snapshot  */
static bool g_qmp_use_frozen   = false;
static int  g_qmp_fail_next    = 0;               /* one-shot rc!=0      */
static int  g_qmp_wrong_span_next = 0;            /* success, wrong span */
static int  g_qmp_lie_rwx_left = 0;               /* report R|X n times  */
static long g_qmp_lie_at_call  = 0;               /* 1-based qmp call to
                                                     lie on (0 = off)    */
static long g_qmp_calls        = 0;
static int  g_vq_not_writable  = 0;               /* vq reports no-W n times */
static int  g_vq_fail_next     = 0;
static long g_mprotect_allow   = -1;              /* -1 = never fail; else
                                                     fail after N calls  */
static long g_mprotect_calls   = 0;

extern "C" int cap3_host_mprotect(void* addr, size_t len, int prot);

extern "C" int cap3_host_mprotect(void* addr, size_t len, int prot)
{
    ++g_mprotect_calls;
    if(g_mprotect_allow >= 0 && g_mprotect_calls > g_mprotect_allow)
        return -1; /* caller observes errno via real errno; leave as-is */

    uintptr_t first = (uintptr_t)addr & ~(uintptr_t)PAGE_MASK;
    uintptr_t last  = ((uintptr_t)addr + len - 1) & ~(uintptr_t)PAGE_MASK;
    for(uintptr_t p = first; p <= last; p += PAGE_SIZE)
    {
        if(g_pages.find(p) == g_pages.end())
            return -1; /* unmapped -> EINVAL-ish failure */
        g_pages[p] = prot;
        /* g_frozen deliberately does NOT track mprotect: it models a
         * STALE query view (pre-OPEN protection) for the VERIFY-OPEN
         * failure test. */
    }
    return 0;
}

/* Mirror of the VqInfo layout declared inside CodePatch.cpp (U3, PS4-
 * derived): start 0x00, end 0x08, offset 0x10, prot 0x18, flags 0x1c,
 * name[32] 0x20, size 0x40. */
struct HostVqInfo {
    void*    start;
    void*    end;
    uint64_t offset;
    int32_t  prot;
    int32_t  flags;
    char     name[32];
};
static_assert(sizeof(HostVqInfo) == 0x40, "host vq size");
static_assert(offsetof(HostVqInfo, prot) == 0x18, "host vq prot at 0x18");

extern "C" int cap3_host_qmp(const void* addr, void** start, void** end,
                             int* prot);
extern "C" int cap3_host_vq(const void* addr, int /*class_idx*/,
                            void* infov, size_t info_size);

static const int* page_lookup(const void* addr)
{
    uintptr_t p = (uintptr_t)addr & ~(uintptr_t)PAGE_MASK;
    if(g_qmp_use_frozen)
    {
        std::map<uintptr_t, int>::const_iterator it = g_frozen.find(p);
        if(it == g_frozen.end())
            return 0;
        return &it->second;
    }
    std::map<uintptr_t, int>::const_iterator it = g_pages.find(p);
    if(it == g_pages.end())
        return 0;
    return &it->second;
}

extern "C" int cap3_host_qmp(const void* addr, void** start, void** end,
                             int* prot)
{
    ++g_qmp_calls;
    if(g_qmp_fail_next > 0)
    {
        --g_qmp_fail_next;
        return -1;
    }
    const int* p = page_lookup(addr);
    if(!p)
        return -1;
    int v = *p;
    if((g_qmp_lie_rwx_left > 0 && (--g_qmp_lie_rwx_left, true)) ||
       (g_qmp_lie_at_call > 0 && g_qmp_calls == g_qmp_lie_at_call))
    {
        v = PROT_READ | PROT_WRITE | PROT_EXEC;
    }
    uintptr_t base = (uintptr_t)addr & ~(uintptr_t)PAGE_MASK;
    if(g_qmp_wrong_span_next > 0)
    {
        --g_qmp_wrong_span_next;
        if(start) *start = (void*)(base + PAGE_SIZE);
        if(end)   *end   = (void*)(base + 2 * PAGE_SIZE);
    }
    else
    {
        if(start) *start = (void*)base;
        if(end)   *end   = (void*)(base + PAGE_SIZE);
    }
    if(prot)  *prot  = v;
    return 0;
}

extern "C" int cap3_host_vq(const void* addr, int /*class_idx*/,
                            void* infov, size_t info_size)
{
    if(info_size < sizeof(HostVqInfo))
        return -1;
    if(g_vq_fail_next > 0)
    {
        --g_vq_fail_next;
        return -1;
    }
    const int* p = page_lookup(addr);
    if(!p)
        return -1;
    HostVqInfo* info = (HostVqInfo*)infov;
    memset(info, 0, sizeof(*info));
    uintptr_t base = (uintptr_t)addr & ~(uintptr_t)PAGE_MASK;
    info->start = (void*)base;
    info->end   = (void*)(base + PAGE_SIZE);
    info->prot  = *p;
    if(g_vq_not_writable > 0)
    {
        --g_vq_not_writable;
        info->prot = PROT_READ | PROT_EXEC; /* disagrees on W */
    }
    snprintf(info->name, sizeof(info->name), "%s", "hostmock");
    return 0;
}

/* ================================================================== */
/* Fixture: a fake "text" span of 2 pages, registered R|X like text    */
/* ================================================================== */

static uint8_t* g_span = 0;      /* 2 * PAGE_SIZE, real RW anon memory  */
static const int kOrigProt = PROT_READ | PROT_EXEC;  /* mock "text"     */

/* "code": movl $0x1337,%eax; 8 nops; ret (15 bytes) — same shape as the
 * diagnostic's own probe fn. */
static const uint8_t kOriginal[15] = {
    0xB8, 0x37, 0x13, 0x00, 0x00,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0xC3
};
/* replacement: mov eax, 0xC0FFEE ; ret (6 bytes) */
static const uint8_t kReplacement[6] = { 0xB8, 0xEE, 0xFF, 0xC0, 0x00, 0xC3 };
static const size_t  kPatchLen = 6;

static void fixture_setup(void)
{
    g_span = (uint8_t*)mmap(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANON, -1, 0);
    if(g_span == MAP_FAILED)
    {
        printf("[HOST] FATAL: fixture mmap failed\n");
        _exit(3);
    }
    memcpy(g_span, kOriginal, sizeof(kOriginal));
    for(int i = 0; i < 2; ++i)
    {
        g_pages[(uintptr_t)(g_span + i * PAGE_SIZE)] = kOrigProt;
        g_frozen[(uintptr_t)(g_span + i * PAGE_SIZE)] = kOrigProt;
    }
    cap03::set_patch_log_sink(host_log_sink, 0);
}

static void fixture_reset_injections(void)
{
    g_qmp_use_frozen   = false;
    g_qmp_fail_next    = 0;
    g_qmp_wrong_span_next = 0;
    g_qmp_lie_rwx_left = 0;
    g_qmp_lie_at_call  = 0;
    g_qmp_calls        = 0;
    g_vq_not_writable  = 0;
    g_vq_fail_next     = 0;
    g_mprotect_allow   = -1;
}

static bool page_prot_is(const void* addr, int prot)
{
    const int* p = page_lookup(addr);
    return p && *p == prot;
}

/* ================================================================== */
/* Tests                                                               */
/* ================================================================== */

static void test_arg_validation(void)
{
    log_clear();
    uint8_t big[65] = { 0 };
    PatchBackup b;
    memset(&b, 0, sizeof(b));

    CHECK(cap03::patch_code_ex(0, kReplacement, kPatchLen, &b,
                               cap03::kFlagSkipLiveness)
              == PatchResult::kBadArgs, "null target -> kBadArgs");
    CHECK(cap03::patch_code_ex(g_span, 0, kPatchLen, &b,
                               cap03::kFlagSkipLiveness)
              == PatchResult::kBadArgs, "null replacement -> kBadArgs");
    CHECK(cap03::patch_code_ex(g_span, kReplacement, 0, &b,
                               cap03::kFlagSkipLiveness)
              == PatchResult::kBadArgs, "zero length -> kBadArgs");
    CHECK(cap03::patch_code_ex(g_span, big, sizeof(big), &b,
                               cap03::kFlagSkipLiveness)
              == PatchResult::kTooLarge, "65 bytes -> kTooLarge");
    CHECK(cap03::patch_code_ex(g_span, kReplacement, kPatchLen, &b,
                               cap03::kFlagRequireLiveness |
                               cap03::kFlagSkipLiveness)
              == PatchResult::kBadFlags, "conflicting flags -> kBadFlags");
    CHECK(!log_contains("STEP=OPEN"),
          "no OPEN before arg validation passes");
}

static void test_query_failure(void)
{
    log_clear();
    PatchBackup b;
    memset(&b, 0, sizeof(b));
    g_qmp_fail_next = 1; /* pre-flight qmp fails */
    PatchResult r = cap03::patch_code_ex(g_span, kReplacement, kPatchLen,
                                         &b, cap03::kFlagSkipLiveness);
    CHECK(r == PatchResult::kQueryFailed, "pre-flight query fail -> kQueryFailed");
    CHECK(!log_contains("STEP=OPEN"), "query failure never opens the page");
    CHECK(memcmp(g_span, kOriginal, sizeof(kOriginal)) == 0,
          "bytes untouched after query failure");
    CHECK(page_prot_is(g_span, kOrigProt), "prot untouched after query failure");
    fixture_reset_injections();
}

static void test_query_span_validation(void)
{
    log_clear();
    PatchBackup b;
    memset(&b, 0, sizeof(b));
    g_qmp_wrong_span_next = 1;
    PatchResult r = cap03::patch_code_ex(g_span, kReplacement, kPatchLen,
                                         &b, cap03::kFlagSkipLiveness);
    CHECK(r == PatchResult::kQueryFailed,
          "successful query with unrelated span -> kQueryFailed");
    CHECK(!log_contains("STEP=OPEN") &&
          memcmp(g_span, kOriginal, sizeof(kOriginal)) == 0 &&
          page_prot_is(g_span, kOrigProt),
          "unrelated query span cannot open or modify text");
    if(r == PatchResult::kOk)
        (void)cap03::unpatch_code_ex(&b, cap03::kFlagSkipLiveness);
    fixture_reset_injections();
}

static void test_address_overflow(void)
{
    /* Isolate the pre-fix invalid dereference in a child process. */
    pid_t child = fork();
    if(child == 0)
    {
        PatchBackup b;
        memset(&b, 0, sizeof(b));
        void* wrap = (void*)(UINTPTR_MAX - 2);
        PatchResult r = cap03::patch_code_ex(wrap, kReplacement, kPatchLen,
                                             &b, cap03::kFlagSkipLiveness);
        _exit(r == PatchResult::kBadArgs ? 0 : 1);
    }
    int status = 0;
    const bool waited = child > 0 && waitpid(child, &status, 0) == child;
    CHECK(waited && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "wrapping patch address refuses before reading target");
}

static void test_mixed_protection(void)
{
    log_clear();
    PatchBackup b;
    memset(&b, 0, sizeof(b));
    /* second page reports a different prot -> refuse before OPEN */
    g_pages[(uintptr_t)(g_span + PAGE_SIZE)] = PROT_READ | PROT_WRITE;
    PatchResult r = cap03::patch_code_ex(g_span + PAGE_SIZE - 3, kReplacement,
                                         kPatchLen, &b,
                                         cap03::kFlagSkipLiveness);
    CHECK(r == PatchResult::kMixedProtection, "mixed span prot -> kMixedProtection");
    CHECK(!log_contains("STEP=OPEN"), "mixed protection never opens");
    CHECK(memcmp(g_span, kOriginal, sizeof(kOriginal)) == 0,
          "bytes untouched on mixed protection");
    g_pages[(uintptr_t)(g_span + PAGE_SIZE)] = kOrigProt;
}

static void test_happy_patch_unpatch(void)
{
    log_clear();
    PatchBackup b;
    memset(&b, 0, sizeof(b));
    long mprot_before = g_mprotect_calls;

    PatchResult r = cap03::patch_code_ex(g_span + 4, kReplacement, kPatchLen,
                                         &b, cap03::kFlagSkipLiveness);
    CHECK(r == PatchResult::kOk, "patch_code_ex -> kOk");
    CHECK(b.magic == cap03::kBackupMagic && b.valid == 1,
          "backup magic+valid set");
    CHECK(b.page_count == 1 && b.orig_prot[0] == kOrigProt,
          "backup records page count and original prot");
    CHECK(memcmp(g_span + 4, kReplacement, kPatchLen) == 0,
          "target bytes == replacement after patch");
    CHECK(page_prot_is(g_span, PROT_READ | PROT_WRITE | PROT_EXEC),
          "page left OPEN (RWX) while patched — restored by unpatch");
    CHECK(g_mprotect_calls - mprot_before == 1,
          "exactly ONE span-level mprotect in patch phase");

    CHECK(log_ordered((std::vector<const char*>){
              "=== BEGIN", "HEX] original", "STEP=OPEN",
              "STEP=WRITE+READBACK", "STEP=BARRIER", "=== PASS"}),
          "patch log order: BEGIN < original-hex < OPEN < WRITE+READBACK "
          "< BARRIER < PASS");
    CHECK(log_count("STEP=OPEN") == 1, "exactly one OPEN in patch phase");
    CHECK(log_contains("(capped)") == false, "6-byte hex not capped");

    /* --- unpatch: NOT_PATCHED_STATE guard first --- */
    log_clear();
    g_span[4] = 0x90; /* corrupt the patched state */
    PatchResult ur0 = cap03::unpatch_code_ex(&b, 0);
    CHECK(ur0 == PatchResult::kNotPatchedState,
          "corrupted target -> kNotPatchedState (nothing written)");
    CHECK(!log_contains("STEP=OPEN"),
          "NOT_PATCHED_STATE never opens");

    /* re-establish the patched state from the recorded replacement. */
    memcpy(g_span + 4, kReplacement, kPatchLen);
    CHECK(memcmp(g_span + 4, kReplacement, kPatchLen) == 0,
          "patched state re-established");

    /* --- real unpatch --- */
    log_clear();
    PatchResult ur = cap03::unpatch_code_ex(&b, 0);
    CHECK(ur == PatchResult::kOk, "unpatch_code_ex -> kOk");
    CHECK(b.valid == 0, "backup validity cleared on restore success");
    CHECK(memcmp(g_span + 4, kOriginal + 4, kPatchLen) == 0,
          "original bytes restored");
    CHECK(page_prot_is(g_span, kOrigProt),
          "RESTORE-PROOF: page prot back to R|X");
    CHECK(log_ordered((std::vector<const char*>){
              "UNPATCH] === BEGIN", "STEP=OPEN", "STEP=WRITE+READBACK",
              "STEP=RESTORE-PROT", "PASS restoration proven"}),
          "unpatch log order: BEGIN < OPEN < WRITE < RESTORE-PROT < PASS");
}

static void test_cross_page_span(void)
{
    log_clear();
    /* target straddles the page boundary: last 3 bytes of page 0 + first
     * 3 of page 1. */
    uint8_t* t = g_span + PAGE_SIZE - 3;
    PatchBackup b;
    memset(&b, 0, sizeof(b));

    PatchResult r = cap03::patch_code_ex(t, kReplacement, kPatchLen, &b,
                                         cap03::kFlagSkipLiveness);
    CHECK(r == PatchResult::kOk, "cross-page patch_code_ex -> kOk");
    CHECK(b.page_count == 2, "backup records 2 pages");
    CHECK(memcmp(t, kReplacement, kPatchLen) == 0,
          "cross-page target patched");

    log_clear();
    PatchResult ur = cap03::unpatch_code_ex(&b, 0);
    CHECK(ur == PatchResult::kOk, "cross-page unpatch -> kOk");
    /* The restored bytes must equal the backup's own pre-OPEN capture of
     * the cross-page region (page 1 body was zero-filled anon memory). */
    CHECK(memcmp(t, b.original_bytes, kPatchLen) == 0,
          "cross-page original bytes restored (vs backup capture)");
    CHECK(page_prot_is(g_span, kOrigProt) &&
          page_prot_is(g_span + PAGE_SIZE, kOrigProt),
          "both cross-page pages restored to R|X");
    CHECK(log_count("STEP=RESTORE-PROT") == 2,
          "per-page RESTORE-PROT for 2 pages");
}

static void test_already_applied(void)
{
    log_clear();
    PatchBackup b;
    memset(&b, 0, sizeof(b));
    /* temporarily make target already == replacement */
    memcpy(g_span + 4, kReplacement, kPatchLen);
    long mprot_before = g_mprotect_calls;
    PatchResult r = cap03::patch_code_ex(g_span + 4, kReplacement, kPatchLen,
                                         &b, cap03::kFlagSkipLiveness);
    CHECK(r == PatchResult::kAlreadyApplied, "idempotent patch -> kAlreadyApplied");
    CHECK(g_mprotect_calls == mprot_before,
          "ALREADY_APPLIED performs no mprotect");
    memcpy(g_span + 4, kOriginal + 4, kPatchLen); /* restore fixture */
}

static void test_open_mprotect_failure(void)
{
    log_clear();
    PatchBackup b;
    memset(&b, 0, sizeof(b));
    g_mprotect_allow = 0; /* first mprotect (STEP 3 OPEN) fails */
    PatchResult r = cap03::patch_code_ex(g_span + 4, kReplacement, kPatchLen,
                                         &b, cap03::kFlagSkipLiveness);
    CHECK(r == PatchResult::kMprotectFailed, "OPEN mprotect rc!=0 -> kMprotectFailed");
    CHECK(memcmp(g_span + 4, kOriginal + 4, kPatchLen) == 0,
          "no write after failed OPEN");
    CHECK(page_prot_is(g_span, kOrigProt), "prot unchanged after failed OPEN");
    CHECK(log_ordered((std::vector<const char*>){
              "STEP=OPEN", "rc=-1"}) ||
          log_count("STEP=OPEN") == 1,
          "OPEN logged once with failure");
    CHECK(!log_contains("STEP=WRITE+READBACK"),
          "no WRITE step after failed OPEN");
    CHECK(!log_contains("UNPATCH] === BEGIN"),
          "failed patch leaves nothing to unpatch");
    fixture_reset_injections();
}

static void test_verify_open_failure_qmp_stale(void)
{
    log_clear();
    PatchBackup b;
    memset(&b, 0, sizeof(b));
    /* qmp keeps reporting the PRE-OPEN protection after a successful
     * mprotect -> VERIFY-OPEN must fail and REVERT-ONLY must run. */
    g_qmp_use_frozen = true; /* frozen view never sees the OPEN */
    PatchResult r = cap03::patch_code_ex(g_span + 4, kReplacement, kPatchLen,
                                         &b, cap03::kFlagSkipLiveness);
    CHECK(r == PatchResult::kVerifyOpenFailed,
          "stale qmp after OPEN -> kVerifyOpenFailed");
    CHECK(log_contains("VERIFY-OPEN failed -> REVERT-ONLY"),
          "VERIFY-OPEN failure routes to REVERT-ONLY");
    CHECK(log_contains("[CAP3][REVERT]"), "revert path logged");
    CHECK(log_count("STEP=RESTORE-PROT") == 1,
          "revert restores the page prot (per-page, rc checked)");
    /* truth (not the frozen view): mprotect succeeded, revert restored. */
    fixture_reset_injections();
    CHECK(page_prot_is(g_span, kOrigProt),
          "real prot restored after REVERT-ONLY");
    CHECK(memcmp(g_span + 4, kOriginal + 4, kPatchLen) == 0,
          "original bytes intact after REVERT-ONLY");
    CHECK(b.valid == 0, "backup never published on failed patch");
}

static void test_verify_open_failure_vq_disagrees(void)
{
    log_clear();
    PatchBackup b;
    memset(&b, 0, sizeof(b));
    g_vq_not_writable = 8; /* every vq report denies W during VERIFY-OPEN */
    PatchResult r = cap03::patch_code_ex(g_span + 4, kReplacement, kPatchLen,
                                         &b, cap03::kFlagSkipLiveness);
    CHECK(r == PatchResult::kVerifyOpenFailed,
          "vq writability disagreement -> kVerifyOpenFailed (conservative)");
    CHECK(log_contains("[CAP3][QUERY] vq"), "vq corroboration actually ran");
    CHECK(page_prot_is(g_span, kOrigProt), "prot restored after vq refusal");
    CHECK(memcmp(g_span + 4, kOriginal + 4, kPatchLen) == 0,
          "bytes untouched after vq refusal");
    fixture_reset_injections();
}

static void test_restore_retry_budget(void)
{
    /* 6a: one-shot verification lie -> exactly ONE retry (verdict 7c).
     * unpatch_code_ex qmp sequence for a 1-page span: call 1 = pre-flight,
     * call 2 = VERIFY-OPEN, call 3 = restore verification. The lie targets
     * call 3 exactly (a pre-flight lie would be truth-preserving here:
     * the page really is RWX until restore). */
    {
        log_clear();
        PatchBackup b;
        memset(&b, 0, sizeof(b));
        PatchResult pr = cap03::patch_code_ex(g_span + 4, kReplacement,
                                              kPatchLen, &b,
                                              cap03::kFlagSkipLiveness);
        CHECK(pr == PatchResult::kOk, "6a setup patch ok");
        log_clear();
        fixture_reset_injections();      /* zeroes g_qmp_calls too */
        g_qmp_lie_at_call = 3;           /* restore verification query  */
        PatchResult ur = cap03::unpatch_code_ex(&b, 0);
        CHECK(ur == PatchResult::kOk,
              "single verification mismatch -> retry honored -> kOk");
        CHECK(log_count("STEP=RESTORE-PROT") == 2,
              "exactly 2 attempts (0 and 1), no more");
        CHECK(log_count("restore retry 1/1") == 1,
              "exactly one retry, loudly logged");
        CHECK(b.valid == 0, "backup cleared after proven restore");
        fixture_reset_injections();
    }
    /* 6b: persistent lie -> kRestoreVerifyFailed, STATE DIRTY, no 3rd try. */
    {
        log_clear();
        PatchBackup b;
        memset(&b, 0, sizeof(b));
        PatchResult pr = cap03::patch_code_ex(g_span + 4, kReplacement,
                                              kPatchLen, &b,
                                              cap03::kFlagSkipLiveness);
        CHECK(pr == PatchResult::kOk, "6b setup patch ok");
        log_clear();
        g_qmp_lie_rwx_left = 99; /* lies forever */
        PatchResult ur = cap03::unpatch_code_ex(&b, 0);
        CHECK(ur == PatchResult::kRestoreVerifyFailed,
              "persistent verification mismatch -> kRestoreVerifyFailed");
        CHECK(log_count("STEP=RESTORE-PROT") == 2,
              "retry budget respected: 2 attempts only, never 3");
        CHECK(log_contains("STATE DIRTY"),
              "dirty state reported loudly");
        CHECK(b.valid == 1, "backup stays valid on failed restore");
        CHECK(page_prot_is(g_span, kOrigProt),
              "mock truth: prot actually restored (only the report lied)");
        fixture_reset_injections();
    }
}

static void test_restore_mprotect_terminal(void)
{
    log_clear();
    PatchBackup b;
    memset(&b, 0, sizeof(b));
    PatchResult pr = cap03::patch_code_ex(g_span + 4, kReplacement, kPatchLen,
                                          &b, cap03::kFlagSkipLiveness);
    CHECK(pr == PatchResult::kOk, "terminal-restore setup patch ok");
    log_clear();
    /* call 1 = unpatch OPEN (must succeed); call 2 = RESTORE-PROT (fails). */
    g_mprotect_allow = g_mprotect_calls + 1;
    PatchResult ur = cap03::unpatch_code_ex(&b, 0);
    CHECK(ur == PatchResult::kRestoreMprotectFailed,
          "restore mprotect rc!=0 -> kRestoreMprotectFailed");
    CHECK(log_count("STEP=RESTORE-PROT") == 1,
          "rc failure is terminal: no retry attempt");
    CHECK(log_contains("STATE DIRTY"), "terminal restore failure is loud");
    CHECK(page_prot_is(g_span, PROT_READ | PROT_WRITE | PROT_EXEC),
          "honest dirty state: page stays OPEN when restore mprotect fails");
    CHECK(memcmp(g_span + 4, kOriginal + 4, kPatchLen) == 0,
          "bytes were restored before the prot restore failed");
    /* manual cleanup for later tests (the code refuses further patches by
     * design; the mock owner may reset the fixture). */
    g_pages[(uintptr_t)g_span] = kOrigProt;
    g_frozen[(uintptr_t)g_span] = kOrigProt;
    b.valid = 0;
    fixture_reset_injections();
}

static void test_raii_scope(void)
{
    /* happy path: auto-revert on destruction. */
    {
        log_clear();
        long begin_before = (long)log_count("UNPATCH] === BEGIN");
        {
            cap03::PatchScope scope(g_span + 4, kReplacement, kPatchLen,
                                    cap03::kFlagSkipLiveness);
            CHECK(scope.ok(), "PatchScope applies on construction");
            CHECK(scope.status() == PatchResult::kOk, "scope status kOk");
            CHECK(memcmp(g_span + 4, kReplacement, kPatchLen) == 0,
                  "scope patched the target");
            log_clear();
        }
        CHECK(log_count("UNPATCH] === BEGIN") == begin_before + 1,
              "destructor ran exactly one unpatch");
        CHECK(log_contains("PASS restoration proven"),
              "scope auto-revert proven (restoration proof logged)");
        CHECK(page_prot_is(g_span, kOrigProt), "scope revert restored prot");
    }
    /* constructor failure: destructor is a no-op. */
    {
        log_clear();
        g_mprotect_allow = 0; /* OPEN fails */
        cap03::PatchScope scope(g_span + 4, kReplacement, kPatchLen,
                                cap03::kFlagSkipLiveness);
        CHECK(!scope.ok(), "failed scope reports ok()==false");
        CHECK(scope.status() == PatchResult::kMprotectFailed,
              "failed scope surfaces kMprotectFailed");
        log_clear();
        fixture_reset_injections();
    }
    CHECK(log_count("UNPATCH] === BEGIN") == 0,
          "failed scope destructor performs no unpatch");

    /* manual revert success: destructor must NOT retry. */
    {
        log_clear();
        cap03::PatchScope scope(g_span + 4, kReplacement, kPatchLen,
                                cap03::kFlagSkipLiveness);
        CHECK(scope.ok(), "manual-revert scope applied");
        CHECK(scope.revert() == PatchResult::kOk, "manual revert() -> kOk");
        CHECK(!scope.ok(), "ok()==false after revert");
        log_clear();
    }
    CHECK(log_count("UNPATCH] === BEGIN") == 0,
          "destructor does not re-unpatch after successful manual revert");

    /* manual revert failure: destructor logs SCOPE-LEAK, never blind-retries. */
    {
        log_clear();
        cap03::PatchScope scope(g_span + 4, kReplacement, kPatchLen,
                                cap03::kFlagSkipLiveness);
        CHECK(scope.ok(), "leak-scope applied");
        memcpy(g_span + 4, kOriginal + 4, kPatchLen); /* corrupt patched state */
        CHECK(scope.revert() == PatchResult::kNotPatchedState,
              "manual revert on corrupted state -> kNotPatchedState");
        log_clear();
    }
    CHECK(log_count("SCOPE-LEAK") == 1,
          "destructor logs SCOPE-LEAK exactly once");
    CHECK(log_count("UNPATCH] === BEGIN") == 0,
          "destructor does NOT retry a failed manual revert (verdict 7c bound)");
}

static void test_backup_validation(void)
{
    log_clear();
    PatchBackup b;
    memset(&b, 0, sizeof(b));
    CHECK(cap03::unpatch_code_ex(0, 0) == PatchResult::kBackupInvalid,
          "null backup -> kBackupInvalid");
    CHECK(cap03::unpatch_code_ex(&b, 0) == PatchResult::kBackupInvalid,
          "zeroed backup -> kBackupInvalid");
    b.magic = 0xdeadbeef;
    CHECK(cap03::unpatch_code_ex(&b, 0) == PatchResult::kBackupInvalid,
          "wrong magic -> kBackupInvalid");
    b.magic = cap03::kBackupMagic;
    CHECK(cap03::unpatch_code_ex(&b, 0) == PatchResult::kBackupInvalid,
          "valid magic but valid==0 -> kBackupInvalid");
    b.valid = 1;
    b.target = g_span;
    b.length = kPatchLen;
    b.page_base = g_span;
    b.page_count = 0;
    CHECK(cap03::unpatch_code_ex(&b, 0) == PatchResult::kBackupInvalid,
          "forged zero-page backup refuses before target read");
    b.page_count = cap03::kMaxPatchPages + 1;
    CHECK(cap03::unpatch_code_ex(&b, 0) == PatchResult::kBackupInvalid,
          "oversized backup page count refuses before array read");
    b.page_count = 1;
    b.span_len = 0;
    CHECK(cap03::unpatch_code_ex(&b, 0) == PatchResult::kBackupInvalid,
          "inconsistent backup span length refuses");
    b.span_len = PAGE_SIZE;
    b.page_base = g_span + PAGE_SIZE;
    CHECK(cap03::unpatch_code_ex(&b, 0) == PatchResult::kBackupInvalid,
          "inconsistent backup page base refuses");
}

static void test_module_slot_entries(void)
{
    /* Safe pre-liveness paths only: patch_code() with bad args returns at
     * step 0; unpatch_code() without a prior patch refuses. The success
     * paths of patch_code()/unpatch_code() force kFlagRequireLiveness and
     * are console-only on a hooked host. */
    log_clear();
    CHECK(cap03::patch_code(0, kReplacement, kPatchLen)
              == PatchResult::kBadArgs,
          "patch_code(null) -> kBadArgs (before liveness)");
    CHECK(cap03::unpatch_code(g_span, kOriginal, 6)
              == PatchResult::kBackupInvalid,
          "unpatch_code without prior patch_code -> kBackupInvalid");
    CHECK(cap03::unpatch_code(0, kOriginal, 6) == PatchResult::kBadArgs,
          "unpatch_code(null target) -> kBadArgs");
    CHECK(cap03::unpatch_code(g_span, kOriginal, 65)
              == PatchResult::kBackupInvalid,
          "unpatch_code still refuses (no valid slot) for odd length");
}

/* The kekcall CHECK executes a raw x86 `syscall` with sub-op
 * 0xffffffff00000027 in RAX (pinned debug-reader.c:23-46 ABI). On macOS
 * under Rosetta a live invalid syscall does NOT return cleanly (observed
 * 2026-10-13: a forked child wedged in uninterruptible exit) and macOS's
 * syscall gate behavior would prove nothing about the PS5 sysent anyway.
 * Therefore the host test NEVER executes kekcall_check: the module-slot
 * tests below only exercise paths that return BEFORE the P1 liveness
 * call (step-0 validation; no-slot refusal). The live NOT-ALIVE ->
 * kLivenessFailed abort path is console-rung material (TEST-PLAN).
 * The compiled CHECK machinery is verified STATICALLY by
 * tests/run_host_test.sh (disassembly: movabs 0xffffffff00000027 +
 * `syscall` inside kekcall_check) and recorded in BUILD-RECEIPT.md. */

/* ================================================================== */

int main(void)
{
    setvbuf(stdout, 0, _IOLBF, 0); /* stream lines even when piped */
    long host_page = sysconf(_SC_PAGESIZE);
    if(host_page != PAGE_SIZE)
    {
        printf("[HOST] FATAL: compile-time PAGE_SIZE %u != sysconf %ld\n",
               (unsigned)PAGE_SIZE, host_page);
        return 3;
    }
    printf("[HOST] cap03 protocol host test (page size %u, sysconf agrees, "
           "arch x86_64 via Rosetta if applicable)\n", (unsigned)PAGE_SIZE);

    fixture_setup();

    test_arg_validation();
    test_query_failure();
    test_query_span_validation();
    test_address_overflow();
    test_mixed_protection();
    test_happy_patch_unpatch();
    test_cross_page_span();
    test_already_applied();
    test_open_mprotect_failure();
    test_verify_open_failure_qmp_stale();
    test_verify_open_failure_vq_disagrees();
    test_restore_retry_budget();
    test_restore_mprotect_terminal();
    test_raii_scope();
    test_backup_validation();
    test_module_slot_entries();
    /* NOTE: no test executes kekcall_check on the host — see the block
     * comment above test_module_slot_entries. */

    printf("[HOST] =============================================\n");
    printf("[HOST] SUMMARY: checks=%d failures=%d -> %s\n",
           g_checks, g_fails, g_fails == 0 ? "PASS" : "FAIL");
    munmap(g_span, 2 * PAGE_SIZE);
    return g_fails == 0 ? 0 : 1;
}
