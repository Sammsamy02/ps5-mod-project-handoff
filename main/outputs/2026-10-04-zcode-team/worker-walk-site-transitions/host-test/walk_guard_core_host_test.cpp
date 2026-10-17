// walk_guard_core_host_test.cpp — worker-walk-site-transitions, 2026-10-04.
//
// Host logic test for the PROSPERO walk-read guard CORE (extracted verbatim from
// ASMHelper.guarded.cpp by run_host_test.sh). Software mprotect model over a
// synthetic 8-page arena (fc1 IGFC1_HOST_TEST pattern). Proves ORDERING, refusal
// and page-granularity behavior of the shipped core bytes — proves nothing about
// kernel behavior. Host-only; no console contact.
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>

// The extracted core expects these (values = pinned-SDK-verified defaults, passed
// as -D flags by run_host_test.sh to mirror build stamping).
#ifndef RC_ASM_WALK_PROT_READ
#define RC_ASM_WALK_PROT_READ 0x1
#endif
#ifndef RC_ASM_WALK_PROT_EXEC
#define RC_ASM_WALK_PROT_EXEC 0x4
#endif
#ifndef RC_ASM_WALK_PAGE_SHIFT
#define RC_ASM_WALK_PAGE_SHIFT 14
#endif

namespace RC::ASM::WalkGuard
{
#include "asm_walk_guard_core.inc"
}

// ---- software mprotect model -------------------------------------------------

struct Op
{
    uint64_t page{};
    uint64_t len{};
    int prot{};
};

struct Model
{
    static constexpr uint64_t kPS = (static_cast<uint64_t>(1)) << RC_ASM_WALK_PAGE_SHIFT;
    static constexpr int kNPAGES = 8;
    static constexpr uint64_t kBase = 0x10000000ull; // page-aligned arena base
    static constexpr int kRx = RC_ASM_WALK_PROT_READ | RC_ASM_WALK_PROT_EXEC;
    static constexpr int kXOnly = RC_ASM_WALK_PROT_EXEC;

    int prot[kNPAGES];
    std::vector<Op> ops;
    int fail_nth_rx = -1;    // 1-based index of the RX-transition call to fail (-1 = never)
    int fail_all_restores = 0; // 1 = every restore (prot==kXOnly) fails
    int rx_call_count = 0;

    Model() { for (int i = 0; i < kNPAGES; ++i) prot[i] = kXOnly; }

    long mprotect(uint64_t page, uint64_t len, int p)
    {
        // Model validity: page-aligned addr, nonzero page-multiple len, in arena.
        if ((page & (kPS - 1)) != 0 || (len & (kPS - 1)) != 0 || len == 0) return -22;
        if (page < kBase || page >= kBase + (uint64_t)kNPAGES * kPS) return -14;
        const uint64_t idx = (page - kBase) / kPS;
        if (idx + len / kPS > (uint64_t)kNPAGES) return -14;

        const bool is_rx = (p == kRx);
        if (is_rx) ++rx_call_count;
        if (is_rx && fail_nth_rx > 0 && rx_call_count == fail_nth_rx)
        {
            ops.push_back(Op{page, len, p}); // refusal still recorded, table unchanged
            return -13;                      // EACCES-class refusal
        }
        if (!is_rx && fail_all_restores) { ops.push_back(Op{page, len, p}); return -5; }

        for (uint64_t i = 0; i < len / kPS; ++i) prot[idx + i] = p;
        ops.push_back(Op{page, len, p});
        return 0;
    }

    static Model* g_model;
    static long trampoline(uint64_t page, uint64_t len, int p) { return g_model->mprotect(page, len, p); }
};
Model* Model::g_model = nullptr;

static RC::ASM::WalkGuard::GuardEnv make_env(Model& m)
{
    Model::g_model = &m;
    return RC::ASM::WalkGuard::GuardEnv{&Model::trampoline};
}

// ---- tiny assertion harness --------------------------------------------------

static int g_failures = 0;
static int g_passes = 0;
static const char* g_test = "";

#define CHECK(cond)                                                             \
    do                                                                          \
    {                                                                           \
        if (cond) { ++g_passes; }                                               \
        else                                                                    \
        {                                                                       \
            ++g_failures;                                                       \
            std::printf("FAIL [%s] %s:%d: %s\n", g_test, __FILE__, __LINE__, #cond); \
        }                                                                       \
    } while (0)

static bool same_ops(const std::vector<Op>& ops, std::initializer_list<Op> want)
{
    if (ops.size() != want.size()) return false;
    size_t i = 0;
    for (const auto& w : want)
    {
        const bool eq = ops[i].page == w.page && ops[i].len == w.len && ops[i].prot == w.prot;
        if (!eq)
        {
            std::printf("   op[%zu] got {page=%llx len=%llx prot=%x} want {page=%llx len=%llx prot=%x}\n",
                        i, (unsigned long long)ops[i].page, (unsigned long long)ops[i].len,
                        (unsigned)ops[i].prot, (unsigned long long)w.page,
                        (unsigned long long)w.len, (unsigned)w.prot);
            return false;
        }
        ++i;
    }
    return true;
}

// nullptr-safe reason comparison (a covered-scope no-op returns nullptr).
static bool is_reason(const char* r, const char* want)
{
    return r != nullptr && std::strcmp(r, want) == 0;
}

using namespace RC::ASM::WalkGuard;

static constexpr uint64_t P0 = Model::kBase;            // page 0
static constexpr uint64_t P1 = Model::kBase + Model::kPS; // page 1
static constexpr uint64_t P2 = Model::kBase + 2 * Model::kPS;
static constexpr uint64_t P7 = Model::kBase + 7 * Model::kPS;

// T1: aligned address -> single-page transition/restore, exact ordering.
static void t1_single_page_ordering()
{
    g_test = "T1 single-page ordering";
    Model m; auto env = make_env(m); GuardContext ctx{};
    {
        Core c;
        const char* r = c.enter(ctx, P0 + 0x8, kWalkReadLen, true, env);
        CHECK(r == nullptr);
        CHECK(c.active);
        CHECK(same_ops(m.ops, {{P0, Model::kPS, Model::kRx}}));
        CHECK(m.prot[0] == Model::kRx);
        // The read would happen HERE, between enter and exit.
        CHECK(c.exit(ctx, env) == 0);
        CHECK(!c.active);
        CHECK(same_ops(m.ops, {{P0, Model::kPS, Model::kRx}, {P0, Model::kPS, Model::kXOnly}}));
        CHECK(m.prot[0] == Model::kXOnly); // exact round trip
        CHECK(ctx.depth == 0);
    }
}

// T2: read window straddling a page boundary -> exactly 2 pages, one range call.
static void t2_boundary_straddle()
{
    g_test = "T2 boundary straddle";
    Model m; auto env = make_env(m); GuardContext ctx{};
    Core c;
    const uint64_t addr = P1 + Model::kPS - 1; // last byte of page 1; +15 spills into page 2
    const char* r = c.enter(ctx, addr, kWalkReadLen, true, env);
    CHECK(r == nullptr);
    CHECK(same_ops(m.ops, {{P1, 2 * Model::kPS, Model::kRx}}));
    CHECK(m.prot[1] == Model::kRx && m.prot[2] == Model::kRx);
    CHECK(c.exit(ctx, env) == 0);
    CHECK(same_ops(m.ops, {{P1, 2 * Model::kPS, Model::kRx}, {P1, 2 * Model::kPS, Model::kXOnly}}));
}

// T2b: window ending exactly at the boundary stays 1 page.
static void t2b_boundary_exact_fit()
{
    g_test = "T2b boundary exact fit";
    Model m; auto env = make_env(m); GuardContext ctx{};
    Core c;
    const uint64_t addr = P1 + Model::kPS - kWalkReadLen; // fits exactly in page 1
    const char* r = c.enter(ctx, addr, kWalkReadLen, true, env);
    CHECK(r == nullptr);
    CHECK(same_ops(m.ops, {{P1, Model::kPS, Model::kRx}}));
    c.exit(ctx, env);
}

// T3: fold OFF -> loud refusal reason, ZERO syscalls.
static void t3_fold_off_refuses_without_syscall()
{
    g_test = "T3 fold-off refusal";
    Model m; auto env = make_env(m); GuardContext ctx{};
    Core c;
    const char* r = c.enter(ctx, P0 + 0x10, kWalkReadLen, false, env);
    CHECK(is_reason(r, "guard_off_no_rx_verdict"));
    CHECK(m.ops.empty());                 // zero syscalls
    CHECK(ctx.depth == 0);
    CHECK(ctx.anomaly_latch == 0);        // refusal does not latch
    CHECK(c.exit(ctx, env) == 0);         // no-op exit
    CHECK(m.ops.empty());
    // The context stays usable for a folded-on guard afterwards.
    Core c2;
    CHECK(c2.enter(ctx, P0, kWalkReadLen, true, env) == nullptr);
    CHECK(c2.exit(ctx, env) == 0);
    CHECK(m.ops.size() == 2);
}

// T4: refused transition -> NO read, NO restore, no latch poisoning.
static void t4_denied_transition_no_read()
{
    g_test = "T4 denied transition";
    Model m; auto env = make_env(m); GuardContext ctx{};
    m.fail_nth_rx = 1;
    Core c;
    const char* r = c.enter(ctx, P0 + 0x20, kWalkReadLen, true, env);
    CHECK(is_reason(r, "rx_transition_denied"));
    CHECK(c.detail == -13);
    CHECK(!c.active);
    CHECK(ctx.depth == 0);
    CHECK(same_ops(m.ops, {{P0, Model::kPS, Model::kRx}})); // the refused call is the ONLY call
    CHECK(m.prot[0] == Model::kXOnly);                      // table unchanged
    CHECK(c.exit(ctx, env) == 0);                           // no restore issued
    CHECK(ctx.anomaly_latch == 0);                          // a DENIAL is not an anomaly
    // A later guard on another page still works (rc path not poisoned).
    m.fail_nth_rx = -1;
    Core c2;
    CHECK(c2.enter(ctx, P2, kWalkReadLen, true, env) == nullptr);
    CHECK(c2.exit(ctx, env) == 0);
}

// T5: failed restore -> anomaly latch; every later read refuses with zero syscalls.
static void t5_restore_anomaly_latch()
{
    g_test = "T5 restore anomaly latch";
    Model m; auto env = make_env(m); GuardContext ctx{};
    m.fail_all_restores = 1;
    Core c;
    CHECK(c.enter(ctx, P0, kWalkReadLen, true, env) == nullptr);
    const long rc = c.exit(ctx, env);
    CHECK(rc != 0);                                        // reported, never thrown
    CHECK(ctx.anomaly_latch == 1);
    CHECK(ctx.depth == 0);                                 // scope bookkeeping unwound
    Core c2;
    const char* r = c2.enter(ctx, P7, kWalkReadLen, true, env);
    CHECK(is_reason(r, "anomaly_latched_earlier_restore_failed"));
    CHECK(m.ops.size() == 2); // RX transition + failed restore; the refused guard added NOTHING
}

// T6: nested guards, same page -> inner no-ops; exactly one transition/restore pair.
static void t6_nested_same_page()
{
    g_test = "T6 nested same page";
    Model m; auto env = make_env(m); GuardContext ctx{};
    Core outer;
    CHECK(outer.enter(ctx, P0 + 0x4, kWalkReadLen, true, env) == nullptr);
    {
        Core inner;
        const char* r = inner.enter(ctx, P0 + 0x40, kWalkReadLen, true, env);
        CHECK(r == nullptr);      // covered by the outer scope
        CHECK(!inner.active);
        CHECK(m.ops.size() == 1); // no second transition
        CHECK(inner.exit(ctx, env) == 0);
        CHECK(m.ops.size() == 1); // no early restore
    }
    CHECK(ctx.depth == 1);
    CHECK(outer.exit(ctx, env) == 0);
    CHECK(same_ops(m.ops, {{P0, Model::kPS, Model::kRx}, {P0, Model::kPS, Model::kXOnly}}));
    CHECK(ctx.depth == 0);
}

// T7: nested guards, different pages -> inner transitions its own page; restore
// order inner-then-outer (stack discipline); both pages back to baseline.
static void t7_nested_different_pages()
{
    g_test = "T7 nested different pages";
    Model m; auto env = make_env(m); GuardContext ctx{};
    Core outer;
    CHECK(outer.enter(ctx, P0, kWalkReadLen, true, env) == nullptr);
    Core inner;
    CHECK(inner.enter(ctx, P2 + 8, kWalkReadLen, true, env) == nullptr);
    CHECK(m.ops.size() == 2);
    CHECK(inner.exit(ctx, env) == 0);
    CHECK(outer.exit(ctx, env) == 0);
    CHECK(same_ops(m.ops, {{P0, Model::kPS, Model::kRx},
                           {P2, Model::kPS, Model::kRx},
                           {P2, Model::kPS, Model::kXOnly},
                           {P0, Model::kPS, Model::kXOnly}}));
    CHECK(ctx.depth == 0);
    for (int i = 0; i < Model::kNPAGES; ++i) CHECK(m.prot[i] == Model::kXOnly);
}

// T8: depth cap -> the 9th DISTINCT-page scope refuses loudly, only 8 transitions issued.
static void t8_depth_cap()
{
    g_test = "T8 depth cap";
    Model m; auto env = make_env(m); GuardContext ctx{};
    Core cores[Model::kNPAGES + 1];
    for (int i = 0; i < Model::kNPAGES; ++i)
    {
        const uint64_t page = Model::kBase + (uint64_t)i * Model::kPS;
        CHECK(cores[i].enter(ctx, page + 4, kWalkReadLen, true, env) == nullptr);
    }
    CHECK(ctx.depth == kMaxGuardDepth);
    const char* r = cores[Model::kNPAGES].enter(ctx, Model::kBase + Model::kNPAGES * Model::kPS,
                                                kWalkReadLen, true, env); // 9th scope, UNCOVERED page
    CHECK(is_reason(r, "guard_depth_exceeded"));
    CHECK(m.ops.size() == (size_t)Model::kNPAGES); // only the 8 admitted transitions
    // Unwind cleanly.
    for (int i = Model::kNPAGES - 1; i >= 0; --i) CHECK(cores[i].exit(ctx, env) == 0);
    CHECK(ctx.depth == 0);
    for (int i = 0; i < Model::kNPAGES; ++i) CHECK(m.prot[i] == Model::kXOnly);
}

// T9: degenerate inputs refuse loudly with zero syscalls.
static void t9_degenerate_inputs()
{
    g_test = "T9 degenerate inputs";
    Model m; auto env = make_env(m); GuardContext ctx{};
    Core c;
    const char* r = c.enter(ctx, P0, 0, true, env); // zero-length read
    CHECK(is_reason(r, "page_range_invalid"));
    r = c.enter(ctx, UINT64_MAX - 2, kWalkReadLen, true, env); // addr+len overflows
    CHECK(is_reason(r, "page_range_invalid"));
    CHECK(m.ops.empty());
    CHECK(c.exit(ctx, env) == 0);
    CHECK(ctx.depth == 0);
}

// T10: recursion-shaped sequence (jmp-chain resolve = sequential guards) keeps the
// map exactly at baseline after every completed scope.
static void t10_sequential_resolves_round_trip()
{
    g_test = "T10 sequential resolves";
    Model m; auto env = make_env(m); GuardContext ctx{};
    const uint64_t chain[4] = {P0 + 0x10, P1 + Model::kPS - 8, P2 + 0x100, P7 + 0x20};
    for (const uint64_t addr : chain)
    {
        Core c;
        CHECK(c.enter(ctx, addr, kWalkReadLen, true, env) == nullptr);
        CHECK(c.exit(ctx, env) == 0);
        CHECK(ctx.depth == 0);
        for (int i = 0; i < Model::kNPAGES; ++i) CHECK(m.prot[i] == Model::kXOnly);
    }
    CHECK(m.ops.size() == 8); // 4 x (RX + restore)
}

int main()
{
    std::printf("host walk-guard core test (page=%d bytes, prot RX=0x%x X-only=0x%x)\n",
                (int)Model::kPS, Model::kRx, Model::kXOnly);
    t1_single_page_ordering();
    t2_boundary_straddle();
    t2b_boundary_exact_fit();
    t3_fold_off_refuses_without_syscall();
    t4_denied_transition_no_read();
    t5_restore_anomaly_latch();
    t6_nested_same_page();
    t7_nested_different_pages();
    t8_depth_cap();
    t9_degenerate_inputs();
    t10_sequential_resolves_round_trip();
    std::printf("passes=%d failures=%d\n", g_passes, g_failures);
    return g_failures == 0 ? 0 : 1;
}
