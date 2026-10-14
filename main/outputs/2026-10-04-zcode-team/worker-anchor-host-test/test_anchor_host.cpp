// test_anchor_host.cpp — worker-anchor-host-test, 2026-10-04.
//
// Host test of the r3 data-window anchor logic extracted VERBATIM from
// outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r3.cpp
// (sha256 8a8c596101c0b2cfe697a3045d1fbfd337bb8bf440b0c39e40a73e42c64ed4a8):
//   frag_anchor.inc = r3 lines 113-156 (DataWindowAnchor .. window_anchor_verify)
//   frag_lambda.inc = r3 lines 219-241 (ops.scan_image_base wrapper lambda)
// Only the ops table and process memory are mocked; the logic under test is
// byte-identical to the shipped bridge (sed-extracted, never retyped).
//
// MOCK MEMORY MODEL: this macOS host denies MAP_FIXED at the guest-real
// addresses (kernel min-address), so raw dereferences in the logic are
// emulated: a SIGSEGV/SIGBUS handler decodes the faulting LDRB (the ONLY
// instruction that can fault on a p[i] read) and serves the byte from a
// shadow range table. Every served/OOB read is logged — each fixture case
// asserts its exact outcome AND its read log (no out-of-bounds reads).

#include <csetjmp>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/ucontext.h>
#include <vector>

#include "live_module_os_ops.hpp"  // real lmf types (/tmp/comp5_inc)

// ---- logic under test: VERBATIM r3 lines 113-156 ---------------------------
namespace {
#include "frag_anchor.inc"
}  // namespace

// ---- shadow memory: registered ranges serve reads; all else is OOB ---------
struct ShadowRange {
    uint64_t start;
    uint64_t end;  // exclusive
    const unsigned char *bytes;
};
static ShadowRange g_ranges[8];
static uint32_t g_range_n = 0;

// async-signal-safe logs (no allocator in the handler)
static uint64_t g_read_addr[4096];
static unsigned g_read_val[4096];
static volatile uint32_t g_read_n = 0;
static volatile uint32_t g_oob_n = 0;

static void shadow_reset()
{
    g_range_n = 0;
    g_read_n = 0;
    g_oob_n = 0;
}
static void shadow_allow(uint64_t start, const unsigned char *bytes,
                         uint32_t len)
{
    g_ranges[g_range_n].start = start;
    g_ranges[g_range_n].end = start + len;
    g_ranges[g_range_n].bytes = bytes;
    ++g_range_n;
}
static const ShadowRange *shadow_find(uint64_t addr)
{
    for (uint32_t i = 0; i < g_range_n; ++i)
        if (addr >= g_ranges[i].start && addr < g_ranges[i].end)
            return &g_ranges[i];
    return nullptr;
}

// ---- fault handler: decode LDRB, serve from shadow, advance pc -------------
static sigjmp_buf g_fault_jmp;
static volatile sig_atomic_t g_fault_state = 0;  // 0 none, 1 served, 2 oob
static volatile uint64_t g_bad_addr = 0;
static volatile uint32_t g_bad_inst = 0;

static auto reg_slot(ucontext_t *u, unsigned n)
{
    auto *ss = &u->uc_mcontext->__ss;
    switch (n)
    {
        case 29: return &ss->__fp;
        case 30: return &ss->__lr;
        default: return &ss->__x[n];
    }
}

static void fault_handler(int sig, siginfo_t *, void *ctx)
{
    (void)sig;
    ucontext_t *u = static_cast<ucontext_t *>(ctx);
    const uint64_t pc = u->uc_mcontext->__ss.__pc;
    const uint32_t inst = *reinterpret_cast<volatile uint32_t *>(pc);

    uint64_t addr;
    bool served_form = false;
    if ((inst & 0xFFC00000u) == 0x39400000u)  // LDRB Wt,[Xn,#imm12]
    {
        addr = *reg_slot(u, (inst >> 5) & 0x1Fu) + ((inst >> 10) & 0xFFFu);
        served_form = true;
    }
    else if ((inst & 0xFFE00C00u) == 0x38600800u)  // LDRB Wt,[Xn,Xm{,ext}]
    {
        const unsigned option = (inst >> 13) & 0x7u;
        const unsigned s = (inst >> 12) & 0x1u;
        const uint64_t base = *reg_slot(u, (inst >> 5) & 0x1Fu);
        const uint64_t off = *reg_slot(u, (inst >> 16) & 0x1Fu);
        if (option == 0x3u)  // LSL, S must be 0 for byte loads
        {
            if (s != 0u) served_form = false;
            else { addr = base + off; served_form = true; }
        }
        else if (option == 0x2u) { addr = base + (off & 0xFFFFFFFFu); served_form = true; }  // UXTW
        else if (option == 0x6u) { addr = base + static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(off))); served_form = true; }  // SXTW
        else if (option == 0x7u) { addr = base + off; served_form = true; }  // SXTX
    }

    if (served_form)
    {
        if (const ShadowRange *r = shadow_find(addr))
        {
            const unsigned value = r->bytes[addr - r->start];
            const unsigned rt = inst & 0x1Fu;
            if (rt < 31u) *reg_slot(u, rt) = value;  // WZR: discard
            if (g_read_n < 4096u)
            {
                g_read_addr[g_read_n] = addr;
                g_read_val[g_read_n] = value;
            }
            ++g_read_n;
            if (g_read_n > 100000u)  // runaway loop guard
            {
                g_fault_state = 3;
                siglongjmp(g_fault_jmp, 1);
            }
            g_fault_state = 1;
            u->uc_mcontext->__ss.__pc = pc + 4;
            return;
        }
        g_bad_addr = addr;
    }
    g_bad_inst = inst;
    g_fault_state = 2;  // OOB read or unknown faulting instruction
    siglongjmp(g_fault_jmp, 1);
}

// ---- mock ops state --------------------------------------------------------
struct Mock {
    std::vector<lmf::OsVmMapping> mappings;
    int query_rc = 0;
    size_t query_total = 0;
    int base_scan_rc = -1;  // -1: original header scan finds nothing (needs-base)
    int base_scan_chosen_index = -1;
    uint64_t base_scan_chosen_base = 0;
    mutable int query_calls = 0;
    mutable int base_scan_calls = 0;
    mutable void *last_scan_ctx = reinterpret_cast<void *>(1);
} g_mock;

static int mock_query_vmentry(void *ctx, lmf::OsVmMapping *out, size_t cap,
                              size_t *count)
{
    (void)ctx;
    ++g_mock.query_calls;
    if (g_mock.query_rc != 0) return g_mock.query_rc;
    const size_t n = g_mock.query_total < cap ? g_mock.query_total : cap;
    for (size_t i = 0; i < n; ++i) out[i] = g_mock.mappings[i];
    *count = g_mock.query_total;  // TOTAL may exceed capacity (header contract)
    return 0;
}

static int mock_base_scan(void *ctx, const lmf::OsVmMapping *mappings,
                          size_t count, lmf::VmScanResult *out)
{
    ++g_mock.base_scan_calls;
    g_mock.last_scan_ctx = ctx;
    (void)mappings;
    (void)count;
    if (g_mock.base_scan_rc != 0 || out == nullptr) return g_mock.base_scan_rc;
    out->chosen_index = g_mock.base_scan_chosen_index;
    out->chosen_base = g_mock.base_scan_chosen_base;
    return g_mock.base_scan_rc;
}

// Build the wrapper ops exactly as r3 lines 217-241 do. base_ops must be the
// CALLER's long-lived object (as in r3, where base_ops lives in the start
// function for the whole stage) because ops.context aliases it.
static lmf::LiveModuleOsOps build_wrapper_ops(lmf::LiveModuleOsOps &base_ops)
{
    // r3 lines 217-218 (verbatim behavior; context aliases the caller's base):
    lmf::LiveModuleOsOps ops = base_ops;
    ops.context = &base_ops;  // SCE ops ignore ctx; our wrapper reads it
    // r3 lines 219-241 (VERBATIM, from frag_lambda.inc):
#include "frag_lambda.inc"
    return ops;
}

// ---- fixture helpers -------------------------------------------------------
static void seed_windows(bool with_e, bool with_a)
{
    shadow_reset();
    if (with_e)
        shadow_allow(kCandidateBase + kDataWindowAnchors[0].vaddr,
                     kDataWindowAnchors[0].bytes, kDataWindowAnchors[0].length);
    if (with_a)
        shadow_allow(kCandidateBase + kDataWindowAnchors[1].vaddr,
                     kDataWindowAnchors[1].bytes, kDataWindowAnchors[1].length);
}

static int guarded_call(const lmf::LiveModuleOsOps &ops,
                        const std::vector<lmf::OsVmMapping> &mappings,
                        lmf::VmScanResult *out)
{
    g_fault_state = 0;
    if (sigsetjmp(g_fault_jmp, 1) == 0)
    {
        return ops.scan_image_base(ops.context, mappings.data(),
                                   mappings.size(), out);
    }
    return -99;  // fault marker
}

static int g_failures = 0;

static void check(bool cond, const char *what, const char *casename)
{
    if (!cond)
    {
        ++g_failures;
        std::printf("FAIL [%s] %s\n", casename, what);
    }
}

static void check_exact_anchor_ok(const lmf::VmScanResult &out, int rc,
                                  const char *casename)
{
    check(rc == 0, "wrapper returned 0", casename);
    check(out.chosen_index == 0, "chosen_index == 0", casename);
    check(out.chosen_base == 0x400000, "chosen_base == 0x400000", casename);
    check(out.chosen_extent == 0xbc602c8, "chosen_extent == 0xbc602c8", casename);
    check(out.candidate_count == 1, "candidate_count == 1", casename);
    check(out.truncated == 0, "truncated == 0", casename);
}

static void check_reads_exact(uint32_t expected, const char *casename)
{
    check(g_read_n == expected, "exact read count served from shadow",
          casename);
    check(g_oob_n == 0, "no out-of-bounds reads", casename);
}

static void report(const char *name, int rc, const lmf::VmScanResult &out)
{
    std::printf("CASE %-4s rc=%-4d%s reads=%u oob=%u out={idx=%d base=%#llx "
                "extent=%#llx n=%u trunc=%d}\n",
                name, rc, g_fault_state == 2 ? " FAULT" : "     ", g_read_n,
                g_oob_n, out.chosen_index, (unsigned long long)out.chosen_base,
                (unsigned long long)out.chosen_extent, out.candidate_count,
                out.truncated);
    check(g_fault_state != 2, "no un-servable fault (OOB/unknown inst)", name);
}

static void run_case(const char *name, const std::vector<lmf::OsVmMapping> &m,
                     bool seed_e, bool seed_a)
{
    seed_windows(seed_e, seed_a);

    lmf::LiveModuleOsOps base_ops{};
    base_ops.query_vmentry = mock_query_vmentry;
    base_ops.scan_image_base = mock_base_scan;
    g_mock = Mock{};
    g_mock.mappings = m;
    g_mock.query_total = m.size();
    lmf::LiveModuleOsOps ops = build_wrapper_ops(base_ops);

    lmf::VmScanResult out{};
    const int rc = guarded_call(ops, g_mock.mappings, &out);
    report(name, rc, out);
}

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[boot] fault-driven shadow-memory host test\n");
    struct sigaction sa{};
    sa.sa_sigaction = fault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);

    const uint64_t e_rt = kCandidateBase + kDataWindowAnchors[0].vaddr;
    const uint64_t a_rt = kCandidateBase + kDataWindowAnchors[1].vaddr;
    const uint64_t e_end = e_rt + kDataWindowAnchors[0].length;

    // NOTE: the task brief's literal span start 0x8018c000 lies ABOVE window
    // E's runtime address 0x400000+0x7dd2a40=0x81d2a40, so windows are NOT
    // covered by that literal span — with it, refusal is the CORRECT outcome
    // (verified: refused, zero reads). Fixtures below use 0x818c000, a start
    // genuinely below E, preserving the intended "big readable span" role.
    const std::vector<lmf::OsVmMapping> big_readable = {
        {0x818c000, 0x100118000, 1}};
    const std::vector<lmf::OsVmMapping> e_read_a_exec = {
        {0x818c000, 0x8200000, 1}, {0xc05f000, 0xc060000, 4}};
    const std::vector<lmf::OsVmMapping> e_edge_exact = {
        {0x818c000, e_end, 1}, {0xc05f000, 0xc100000, 1}};
    const std::vector<lmf::OsVmMapping> e_edge_short = {
        {0x818c000, e_end - 1, 1}, {0xc05f000, 0xc100000, 1}};
    const std::vector<lmf::OsVmMapping> prot3_prot5 = {
        {e_rt - 0x1000, e_rt + 0x2000, 3}, {a_rt - 0x1000, a_rt + 0x2000, 5}};

    // (a) happy path: both windows inside one big readable span
    run_case("a", big_readable, true, true);
    {
        seed_windows(true, true);
        lmf::LiveModuleOsOps base_ops{};
        base_ops.query_vmentry = mock_query_vmentry;
        base_ops.scan_image_base = mock_base_scan;
        g_mock = Mock{};
        g_mock.mappings = big_readable;
        g_mock.query_total = 1;
        lmf::LiveModuleOsOps ops = build_wrapper_ops(base_ops);
        lmf::VmScanResult out{};
        const int rc = guarded_call(ops, g_mock.mappings, &out);
        check_exact_anchor_ok(out, rc, "a");
        check_reads_exact(64 + 36, "a");  // E compared fully, then A fully
        check(g_read_addr[0] == e_rt, "first read is E[0] at candidate base",
              "a");
        report("a", rc, out);
    }

    // (b) one byte flipped in window E -> REFUSED; only E bytes read
    run_case("b", big_readable, true, true);
    {
        // serve E with a corrupted copy (flip byte 17)
        static unsigned char e_bad[64];
        std::memcpy(e_bad, kDataWindowAnchors[0].bytes, 64);
        e_bad[17] ^= 0xFF;
        shadow_reset();
        shadow_allow(e_rt, e_bad, 64);
        shadow_allow(a_rt, kDataWindowAnchors[1].bytes,
                     kDataWindowAnchors[1].length);
        lmf::LiveModuleOsOps base_ops{};
        base_ops.query_vmentry = mock_query_vmentry;
        base_ops.scan_image_base = mock_base_scan;
        g_mock = Mock{};
        g_mock.mappings = big_readable;
        g_mock.query_total = 1;
        lmf::LiveModuleOsOps ops = build_wrapper_ops(base_ops);
        lmf::VmScanResult out{};
        const int rc = guarded_call(ops, g_mock.mappings, &out);
        check(rc != 0, "wrapper refused (nonzero)", "b");
        bool a_touched = false;
        for (uint32_t i = 0; i < g_read_n && i < 4096u; ++i)
            if (g_read_addr[i] >= a_rt) a_touched = true;
        check(!a_touched, "refused before any read of window A", "b");
        report("b", rc, out);
    }

    // (c) E readable-covered, A in EXEC span (prot=4) -> REFUSED, no read of A
    run_case("c", e_read_a_exec, true, false);  // A not even in shadow
    {
        check(g_mock.query_calls == 1, "one fresh vmentry fetch", "c");
        bool a_touched = false;
        for (uint32_t i = 0; i < g_read_n && i < 4096u; ++i)
            if (g_read_addr[i] >= a_rt) a_touched = true;
        check(!a_touched, "NO read attempted of A (exec span refused first)",
              "c");
        check_reads_exact(64, "c");
    }

    // (d) query_vmentry fails -> REFUSED with zero reads
    {
        seed_windows(false, false);
        lmf::LiveModuleOsOps base_ops{};
        base_ops.query_vmentry = mock_query_vmentry;
        base_ops.scan_image_base = mock_base_scan;
        g_mock = Mock{};
        g_mock.mappings = big_readable;
        g_mock.query_total = 1;
        g_mock.query_rc = 5;
        lmf::LiveModuleOsOps ops = build_wrapper_ops(base_ops);
        lmf::VmScanResult out{};
        const int rc = guarded_call(ops, g_mock.mappings, &out);
        check(rc != 0, "wrapper refused (nonzero)", "d");
        check(g_read_n == 0, "zero reads when vmentry fails", "d");
        report("d", rc, out);
    }

    // (e) zero mappings -> REFUSED with zero reads
    {
        seed_windows(false, false);
        lmf::LiveModuleOsOps base_ops{};
        base_ops.query_vmentry = mock_query_vmentry;
        base_ops.scan_image_base = mock_base_scan;
        g_mock = Mock{};
        g_mock.query_total = 0;
        lmf::LiveModuleOsOps ops = build_wrapper_ops(base_ops);
        lmf::VmScanResult out{};
        const int rc = guarded_call(ops, g_mock.mappings, &out);
        check(rc != 0, "wrapper refused (nonzero)", "e");
        check(g_read_n == 0, "zero reads with zero mappings", "e");
        report("e", rc, out);
    }

    // (f1) span end == E end exactly -> succeeds; reads stop exactly at edge
    run_case("f1", e_edge_exact, true, true);
    {
        seed_windows(true, true);
        lmf::LiveModuleOsOps base_ops{};
        base_ops.query_vmentry = mock_query_vmentry;
        base_ops.scan_image_base = mock_base_scan;
        g_mock = Mock{};
        g_mock.mappings = e_edge_exact;
        g_mock.query_total = 2;
        lmf::LiveModuleOsOps ops = build_wrapper_ops(base_ops);
        lmf::VmScanResult out{};
        const int rc = guarded_call(ops, g_mock.mappings, &out);
        check_exact_anchor_ok(out, rc, "f1");
        check_reads_exact(64 + 36, "f1");
        uint64_t max_e = 0;
        for (uint32_t i = 0; i < g_read_n && i < 4096u; ++i)
            if (g_read_addr[i] >= e_rt && g_read_addr[i] < e_end &&
                g_read_addr[i] > max_e)
                max_e = g_read_addr[i];
        check(max_e == e_end - 1,
              "highest E read is the last byte; nothing beyond span end",
              "f1");
        report("f1", rc, out);
    }

    // (f2) span ends one byte short of E end -> REFUSED, zero reads
    run_case("f2", e_edge_short, true, true);
    {
        check(g_read_n == 0, "coverage gap refused before any byte read",
              "f2");
        check(g_oob_n == 0, "no OOB reads", "f2");
    }

    // (g) original header scan anchors -> passthrough, window verify NOT run
    {
        seed_windows(true, true);
        lmf::LiveModuleOsOps base_ops{};
        base_ops.query_vmentry = mock_query_vmentry;
        base_ops.scan_image_base = mock_base_scan;
        g_mock = Mock{};
        g_mock.mappings = big_readable;
        g_mock.query_total = 1;
        g_mock.base_scan_rc = 0;
        g_mock.base_scan_chosen_index = 1;
        g_mock.base_scan_chosen_base = 0x3000000;
        lmf::LiveModuleOsOps ops = build_wrapper_ops(base_ops);
        lmf::VmScanResult out{};
        const int rc = guarded_call(ops, g_mock.mappings, &out);
        check(rc == 0, "wrapper returned 0", "g");
        check(out.chosen_index == 1, "chosen_index passed through (=1)", "g");
        check(out.chosen_base == 0x3000000, "chosen_base passed through", "g");
        check(out.chosen_extent == 0, "fallback extent NOT applied", "g");
        check(g_mock.query_calls == 0,
              "window_anchor_verify NOT run (query_vmentry never called)",
              "g");
        check(g_read_n == 0, "no shadow reads on passthrough", "g");
        std::printf("CASE g    rc=%d out={idx=%d base=%#llx} query_calls=%d "
                    "base_scan_calls=%d (ctx passed to base scan: %s)\n",
                    rc, out.chosen_index, (unsigned long long)out.chosen_base,
                    g_mock.query_calls, g_mock.base_scan_calls,
                    g_mock.last_scan_ctx == nullptr ? "nullptr" : "non-null");
    }

    // (h) prot=3 (rw) and prot=5 (r-x) both count as readable (bit0 set)
    run_case("h", prot3_prot5, true, true);
    {
        seed_windows(true, true);
        lmf::LiveModuleOsOps base_ops{};
        base_ops.query_vmentry = mock_query_vmentry;
        base_ops.scan_image_base = mock_base_scan;
        g_mock = Mock{};
        g_mock.mappings = prot3_prot5;
        g_mock.query_total = 2;
        lmf::LiveModuleOsOps ops = build_wrapper_ops(base_ops);
        lmf::VmScanResult out{};
        const int rc = guarded_call(ops, g_mock.mappings, &out);
        check_exact_anchor_ok(out, rc, "h");
        check_reads_exact(64 + 36, "h");
        report("h", rc, out);
    }

    // (i) extra: TOTAL exceeds the 256-entry capacity -> clamp still works
    {
        std::vector<lmf::OsVmMapping> m(256, lmf::OsVmMapping{0, 0, 0});
        m[0] = big_readable[0];
        m[255] = big_readable[0];
        run_case("i", m, true, true);
        check(g_read_n == 64 + 36,
              "clamped scan still compared both windows", "i");
    }

    std::printf(g_failures == 0 ? "\nALL CASES PASS (%d failures)\n"
                                : "\nFAILED (%d failures)\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
