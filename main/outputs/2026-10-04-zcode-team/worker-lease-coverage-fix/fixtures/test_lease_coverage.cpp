// test_lease_coverage.cpp — worker-lease-coverage-fix fixture suite.
//
// Compiles the ACTUAL production bridge TU (r13 = OLD flow, r14 = NEW flow)
// into this test binary — no replicas of the bridge. The PLH OwnedMemory
// service is compiled from its REAL source (OwnedMemory.cpp,
// -DPLH_OWNED_MEMORY_HOST_TEST), so every ReadLease below is the production
// implementation whose throw site (OwnedMemory.cpp:122 "Owned read range
// includes unregistered bytes") launch-8 hit in-game.
//
// Boundaries mocked (same recipe as host_test_r13/test_r13.cpp): sysctl
// (link-symbol override, per-call-count mutation for the drift scenario),
// diagnostic_event (capture), SIGSEGV shadow layer for guest-address reads
// inside window_anchor_verify, and the start-chain symbols (lmf supplier,
// RC conversion/context, Start/Stop ack, ActivationDeleter) as recording
// mocks. Scenario = argv[1]; one process per scenario isolates the
// process-lifetime PLH service singleton.
//
// The "adapter derivation replica" below mirrors — and cites — the shipped
// composed units byte-for-byte in behavior:
//   * anchor set: records of the captured VMMAP containing observed_base
//     (live_module_facts.cpp:449-457, kMaxAnchorMappings=8 cap),
//   * lease ranges: those records filtered to kve-readable, clamped to the
//     extent, sorted, adjacency-merged (lmf_to_native_loader_facts.cpp:77-109,
//     comp6 Diff A protection filter at :90),
// which is what ModuleSnapshot::ReadableRanges().at(MainExe) hands to the
// production ReadLease (OwnedMappedImageCaller.hpp:14-20,55).
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <setjmp.h>
#include <string>
#include <sys/ucontext.h>
#include <vector>

// ---- diagnostic capture ----
struct Ev { const char *tag; uint32_t value; };
static std::vector<Ev> g_evs;
extern "C" void diagnostic_event(const char *tag, uint32_t value) { g_evs.push_back({tag, value}); }
static uint32_t last_ev(const char *tag) {
    for (auto it = g_evs.rbegin(); it != g_evs.rend(); ++it) if (!strcmp(it->tag, tag)) return it->value;
    return 0xdeadbeef;
}
static bool ev_seen(const char *tag) {
    for (const auto &e : g_evs) if (!strcmp(e.tag, tag)) return true;
    return false;
}

// ---- sysctl mock: fixture records with variable kve_structsize ----
static std::vector<unsigned char> g_vm_fixture;      // raw record bytes
static int g_sysctl_fail = 0;
// Drift scenario plumbing: the launch-8 shape is TWO DIFFERENT kernel states
// observed at two different times — T1 (pre-capture registration sysctl in the
// OLD flow) with exec-only text, T2 (capture) with the same record readable.
// The OLD build makes that first extra sysctl call; the NEW build's first and
// only observation IS the capture (its registration derives from the captured
// array), so the NEW fixture presents the drifted T2 state from call one.
static int g_sysctl_calls = 0;
static bool g_drift = false;
static void apply_drift();  // defined below the record builders
extern "C" int sysctl(int *, u_int, void *oldp, size_t *oldlenp, void *, size_t) {
    if (g_sysctl_fail || oldp == nullptr || oldlenp == nullptr) { if (oldlenp) *oldlenp = 0; return -1; }
    ++g_sysctl_calls;
#ifdef LEASE_TEST_OLD
    if (g_drift && g_sysctl_calls >= 2) apply_drift();  // T1 = first call, T2 after
#else
    if (g_drift && g_sysctl_calls >= 1) apply_drift();  // capture is observation #1
#endif
    size_t n = g_vm_fixture.size() < *oldlenp ? g_vm_fixture.size() : *oldlenp;
    memcpy(oldp, g_vm_fixture.data(), n);
    *oldlenp = n;
    return 0;
}
static void put32(std::vector<unsigned char> &b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xff); }
static void put64(std::vector<unsigned char> &b, uint64_t v) { for (int i = 0; i < 8; ++i) b.push_back((v >> (8 * i)) & 0xff); }
// record builder: ksz, span, prot, optional path bytes at offset 136
static void rec(std::vector<unsigned char> &b, uint32_t ksz, uint64_t s, uint64_t e, uint32_t prot,
                const char *path = nullptr) {
    size_t at = b.size(); b.resize(at + ksz, 0);
    auto w32 = [&](uint64_t off, uint32_t v) { memcpy(b.data() + at + off, &v, 4); };
    auto w64 = [&](uint64_t off, uint64_t v) { memcpy(b.data() + at + off, &v, 8); };
    w32(0, ksz); w64(8, s); w64(16, e); w32(56, prot);
    if (path) memcpy(b.data() + at + 136, path, strlen(path) < ksz - 136 ? strlen(path) : ksz - 136);
}

// ---- shadow memory (proven pattern, worker-anchor-host-test) ----
struct SR { uint64_t s, e; const unsigned char *b; };
static SR g_sr[8]; static unsigned g_srn = 0;
static void shadow_reset() { g_srn = 0; }
static void shadow(uint64_t s, const unsigned char *bytes, uint32_t n) { g_sr[g_srn++] = {s, s + n, bytes}; }
static const SR *sfind(uint64_t a) { for (unsigned i = 0; i < g_srn; ++i) if (a >= g_sr[i].s && a < g_sr[i].e) return &g_sr[i]; return nullptr; }
static sigjmp_buf g_jmp;
static auto rslot(ucontext_t *u, unsigned n) { auto *ss = &u->uc_mcontext->__ss; return n == 29 ? &ss->__fp : n == 30 ? &ss->__lr : &ss->__x[n]; }
static void fh(int, siginfo_t *, void *ctx) {
    ucontext_t *u = (ucontext_t *)ctx;
    const uint64_t pc = u->uc_mcontext->__ss.__pc;
    const uint32_t inst = *(volatile uint32_t *)pc;
    uint64_t addr; bool form = false;
    if ((inst & 0xFFC00000u) == 0x39400000u) { addr = *rslot(u, (inst >> 5) & 31) + ((inst >> 10) & 0xFFF); form = true; }
    else if ((inst & 0xFFE00C00u) == 0x38400000u) {  // LDURB Wt,[Xn,#imm9]
        const int64_t imm9 = (int64_t)((inst >> 12) & 0x1FF);
        addr = *rslot(u, (inst >> 5) & 31) + (imm9 < 256 ? imm9 : imm9 - 512);
        form = true;
    }
    else if ((inst & 0xFFE00C00u) == 0x38600800u) {
        const unsigned opt = (inst >> 13) & 7, s = (inst >> 12) & 1;
        const uint64_t b = *rslot(u, (inst >> 5) & 31), o = *rslot(u, (inst >> 16) & 31);
        if (opt == 3 && !s) { addr = b + o; form = true; }
    }
    if (form) { if (const SR *r = sfind(addr)) { const unsigned rt = inst & 31; if (rt < 31) *rslot(u, rt) = r->b[addr - r->s]; u->uc_mcontext->__ss.__pc = pc + 4; return; } }
    siglongjmp(g_jmp, 1);
}

// ---- host shims for PS5 kernel ABI names the production TU uses ----
#include <sys/sysctl.h>
#ifndef KERN_PROC_VMMAP
#define KERN_PROC_VMMAP 35  // PS5 value; the sysctl mock ignores mib anyway
#endif
struct kinfo_vmentry { char _host_size_shim[1160]; };  // sizeof() only; parsing is raw-byte

// ---- THE PRODUCTION BRIDGE (actual source; OLD=r13 / NEW=r14) ----
#if defined(LEASE_TEST_OLD)
#include LEASE_TEST_BRIDGE_TU_OLD
#else
#include LEASE_TEST_BRIDGE_TU_NEW
#endif
#include <polyhook2/OwnedMemory.hpp>

// ============================================================================
// start-chain mocks (host only; identical in spirit to host_test_r13)
// ============================================================================
struct mfa_activation { int alive; };
static unsigned g_mfa_releases = 0;
extern "C" void mfa_release(mfa_activation *a) { if (a != nullptr) ++g_mfa_releases; }
void modbridge::native::ActivationDeleter::operator()(mfa_activation *activation) const noexcept
{
    mfa_release(activation);
}

lmf::LiveModuleOsOps lmf::make_sce_live_module_os_ops(void *) noexcept
{
    return lmf::LiveModuleOsOps{};  // all ops unbound: the bridge wraps them
}

lmf::RetainedLiveModuleFactsSupplier::RetainedLiveModuleFactsSupplier(
    const lmf::LiveModuleOsOps &ops, lmf::LiveFirmwareProfile) noexcept
    : ops_(ops) {}
lmf::RetainedLiveModuleFactsSupplier::~RetainedLiveModuleFactsSupplier() {}
lmf::Status lmf::RetainedLiveModuleFactsSupplier::capture(const char *) noexcept
{
    // Emulate the production capture op order (live_module_facts.cpp:203-207
    // then :298-300): ONE query_vmentry into a supplier-sized buffer, then
    // scan_image_base over the SAME frozen array. This is what makes the r14
    // wrapper-side registration (and the L3 gate) actually run under test —
    // a no-op capture mock would silently bypass the entire r14 flow.
    static lmf::OsVmMapping mappings[1024];  // kMaxVmMappings
    size_t total = 0;
    if (ops_.query_vmentry != nullptr)
        (void)ops_.query_vmentry(ops_.context, mappings, 1024, &total);
    const size_t copied = total < 1024 ? total : 1024;
    if (ops_.scan_image_base != nullptr) {
        lmf::VmScanResult scan{};
        (void)ops_.scan_image_base(ops_.context, mappings, copied, &scan);
    }
    return lmf::Status::Ok;
}
const lmf::LiveModuleFactsSnapshot &lmf::RetainedLiveModuleFactsSupplier::facts() const noexcept
{
    static lmf::LiveModuleFactsSnapshot snapshot{};
    return snapshot;
}

RC::Platform::LmfConversionResult RC::Platform::TryMakeNativeLoaderFactsFromSupplier(
    std::shared_ptr<const lmf::RetainedLiveModuleFactsSupplier>,
    const std::filesystem::path &module_file_path,
    const RC::Platform::LmfConversionOptions &) noexcept
{
    (void)module_file_path;
    RC::Platform::LmfConversionResult r{};
    r.converted = true;
    return r;
}

RC::Platform::NativeProgramContext RC::Platform::MakeNativeProgramContext(RC::Platform::NativeLoaderFacts)
{
    return RC::Platform::NativeProgramContext{};
}

static modbridge::native::ActivatedMods g_start_activation{nullptr};
modbridge::native::RuntimeOwner::RuntimeOwner(std::unique_ptr<RuntimeBackend>,
                                              std::shared_ptr<RuntimeAckSink>) {}
modbridge::native::RuntimeOwner::~RuntimeOwner() {}
std::unique_ptr<modbridge::native::RuntimeOwner>
modbridge::native::StartActivatedRuntimeWithAck(RC::Platform::NativeProgramContext,
                                                ActivatedMods activation,
                                                std::string_view,
                                                const char *)
{
    g_start_activation = std::move(activation);
    return std::unique_ptr<RuntimeOwner>(new RuntimeOwner(nullptr, nullptr));
}
bool modbridge::native::StopRuntimeWithAck(std::unique_ptr<RuntimeOwner> &owner)
{
    owner.reset();
    return true;
}

// ============================================================================
// fixture geometry — TRUE phdr table (retained eboot sha 3b6cfad4..., phnum=14,
// 5 PT_LOADs; controller-parsed) under the load bias 0x400000 CONFIRMED for
// LOAD0/LOAD1, plus the live-observed RW fragment for LOAD7
// ([0x970c000,0x9a34000), which does NOT fit file+0x400000 — runtime spans
// come from the invocation's VMMAP, never phdr arithmetic alone):
//   LOAD0 --X  file [0x0,0x7479b1c)        runtime [0x400000,0x7879b1c)
//              observed prot=4 page-rounded [0x400000,0x787c000)
//   LOAD1 R--  file [0x747c000,0x7eba918)  runtime [0x787c000,0x82ba918)
//   LOAD3 RW-  file [0x7ebc000,0x913c000)  runtime [0x82bc000,0x953c000)
//   LOAD7 RW-  fragment                    runtime [0x970c000,0x9a34000) (observed)
//   LOAD8 ---  file [0x9633420,0xbc602c8)  runtime [0x9a34000,0xc0602c8) NOREAD tail
// extent end 0xc0602c8 == kCandidateBase + kImageExtentVaddr (shipped r13/r14
// constant 0xbc602c8 = max(p_vaddr+p_memsz); NOT page-aligned).
// The armed seed FName ctor sits at runtime 0x63d590 — INSIDE exec-only LOAD0,
// deliberately unregistered (the dormant-throw hardening target).
// ============================================================================
static constexpr uint64_t RT_TEXT_B = 0x400000;
static constexpr uint64_t RT_TEXT_E = 0x787c000;      // LOAD0 page-rounded, prot=4
static constexpr uint64_t RT_RO_B   = 0x787c000;      // LOAD1 R
static constexpr uint64_t RT_RO_E   = 0x82ba918;      // unaligned end
static constexpr uint64_t RT_RW_B   = 0x82bc000;      // LOAD3 RW
static constexpr uint64_t RT_RW_E   = 0x953c000;
static constexpr uint64_t RT_RW7_B  = 0x970c000;      // LOAD7 observed fragment
static constexpr uint64_t RT_RW7_E  = 0x9a34000;
static constexpr uint64_t RT_TAIL_E = 0xc0602c8;      // extent end (NOREAD tail)
static constexpr uint64_t SEED_FNAME = 0x63d590;      // armed seed, exec-only pages

// production window bytes (retained image, C2-verified; r13-test lineage)
static const unsigned char kE[64] = {0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75};
static const unsigned char kD[19] = {'C','u','r','r','e','n','t',' ','C','h','a','n','g','e','l','i','s','t',0};
static const unsigned char kB[8] = {0x55,0,0x45,0,0x35,0,0,0};
static const unsigned char kC[32] = {0x42,0,0x75,0,0x69,0,0x6c,0,0x64,0,0x49,0,0x64,0,0x4f,0,0x76,0,0x65,0,0x72,0,0x72,0,0x69,0,0x64,0,0x65,0,0,0};

static void shadow_all() {
    static unsigned char ebuf[64], dbuf[19], bbuf[8], cbuf[32], c2buf[32];
    memcpy(ebuf, kE, 64); memcpy(dbuf, kD, 19); memcpy(bbuf, kB, 8); memcpy(cbuf, kC, 32); memcpy(c2buf, kC, 32);
    shadow(0x81d2a40, ebuf, 64);   // E rt (LOAD1)
    shadow(0x7f8845d, dbuf, 19);   // D rt (LOAD1)
    shadow(0x7c26782, bbuf, 8);    // B rt (LOAD1)
    shadow(0x79cffda, cbuf, 32);   // C site 1 rt (LOAD1)
    shadow(0x7ac8e58, c2buf, 32);  // C site 2 rt (LOAD1)
}

// true-phdr runtime fixture; text prot selector models the kernel state
static void build_phdr_fixture(uint32_t text_prot) {
    g_vm_fixture.clear();
    rec(g_vm_fixture, 160, RT_TEXT_B, RT_TEXT_E, text_prot, "/app0/eboot.bin"); // LOAD0 (160B: complete kve_path)
    rec(g_vm_fixture, 144, RT_RO_B, RT_RO_E, 1);    // LOAD1 R (unaligned end)
    rec(g_vm_fixture, 144, RT_RW_B, RT_RW_E, 3);    // LOAD3 RW
    rec(g_vm_fixture, 144, RT_RW7_B, RT_RW7_E, 3);  // LOAD7 observed fragment
    rec(g_vm_fixture, 144, RT_RW7_E, RT_TAIL_E, 0); // LOAD8 NOREAD tail to extent end
}
static void apply_drift() {
    // T2 state: the base-containing (text) record turned readable-exec.
    build_phdr_fixture(5);
}

// ---- adapter derivation replica (see file head for the mirrored sites) ----
struct U64Range { uint64_t b, e; };
static std::vector<U64Range> derive_anchor_lease_ranges(uint64_t observed_base) {
    // Parse the raw fixture exactly like bridge_vmmap_walk (kve fields at
    // 0/8/16/56, variable structsize stride).
    struct Parsed { uint64_t s, e; uint32_t prot; };
    std::vector<Parsed> parsed;
    const unsigned char *buf = g_vm_fixture.data();
    uint64_t off = 0, len = g_vm_fixture.size();
    while (off + 64 <= len) {
        uint32_t ksz; memcpy(&ksz, buf + off, 4);
        if (ksz < 64) break;
        Parsed p{};
        memcpy(&p.s, buf + off + 8, 8);
        memcpy(&p.e, buf + off + 16, 8);
        memcpy(&p.prot, buf + off + 56, 4);
        parsed.push_back(p);
        off += ksz;
    }
    // anchor_mappings: records containing observed_base (cap 8; live_module_facts.cpp:449-457)
    std::vector<U64Range> out;
    const uint64_t eb = kCandidateBase, ee = kCandidateBase + kImageExtentVaddr;
    for (const auto &m : parsed) {
        if (!(m.s <= observed_base && observed_base < m.e)) continue;
        if ((m.prot & 1u) == 0) continue;  // comp6 Diff A filter (lmf...cpp:90)
        const uint64_t b = m.s < eb ? eb : m.s;
        const uint64_t e = m.e > ee ? ee : m.e;
        if (e <= b) continue;
        out.push_back({b, e});
        if (out.size() >= 8) break;        // kMaxAnchorMappings
    }
    std::sort(out.begin(), out.end(), [](const U64Range &a, const U64Range &c) { return a.b < c.b; });
    // adjacency/overlap merge (lmf...cpp:100-107)
    std::vector<U64Range> merged;
    for (const auto &r : out) {
        if (!merged.empty() && r.b <= merged.back().e) merged.back().e = r.e > merged.back().e ? r.e : merged.back().e;
        else merged.push_back(r);
    }
    return merged;
}

static int fails = 0;
static void chk(const char *n, bool ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", n); if (!ok) fails++; }

// drive the REAL start entry with the current fixture (mocked chain)
static int drive_start(char *detail, unsigned cap) {
    mfa_activation act{1};
    const unsigned before = g_mfa_releases;
    int rc = modbridge_runtime_start_with_activation(
        "/data/plugins/modbridge_supplier_composition.prx", "/data/modbridge/roots/PPSA33286",
        "PPSA33286", &act, detail, cap);
    if (g_start_activation) { g_start_activation.reset(); ++g_mfa_releases; }
    (void)before;
    return rc;
}

int main(int argc, char **argv) {
    const char *scenario = argc > 1 ? argv[1] : "happy";
    struct sigaction sa {}; sa.sa_sigaction = fh; sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr); sigaction(SIGBUS, &sa, nullptr);
    if (sigsetjmp(g_jmp, 1)) { printf("FAIL unexpected fault in %s\n", scenario); return 2; }

    char detail[256];

    if (!strcmp(scenario, "happy")) {
        // True-phdr shapes, kernel state stable and base record READABLE-EXEC
        // (prot=5: the state where the honest derivation is non-empty). No
        // drift: OLD's pre-capture sysctl and NEW's capture see the SAME
        // bytes, so both builds must cover every derivable lease range.
        build_phdr_fixture(5);
        shadow_all();
        const int rc = drive_start(detail, sizeof detail);
        chk("happy: start reaches MRS_OK", rc == MRS_OK);
        chk("happy: 4 readable spans seen (text RX/LOAD1/LOAD3/LOAD7frag)",
            last_ev("bridge_lease_reg_spans") == 4);
        chk("happy: 4 regions registered (NOREAD tail excluded)",
            last_ev("bridge_lease_reg") == 4);

#ifndef LEASE_TEST_OLD
        chk("happy: no coverage gap raised", !ev_seen("bridge_lease_gap"));
        chk("happy: registry holds 4 spans", g_lease_span_count == 4);
        chk("happy: NOREAD tail NOT registered",
            bridge_lease_span_containing(0x9a34000, 0x4000) == nullptr);
        chk("happy: LOAD1 covered incl. unaligned end (r11 quantization)",
            bridge_lease_span_containing(RT_RO_B, RT_RO_E - RT_RO_B) != nullptr);
        chk("happy: LOAD3 covered", bridge_lease_span_containing(RT_RW_B, RT_RW_E - RT_RW_B) != nullptr);
        chk("happy: LOAD7 fragment covered", bridge_lease_span_containing(RT_RW7_B, RT_RW7_E - RT_RW7_B) != nullptr);
#endif
        // The REAL production lease over the adapter-derived anchor ranges.
        const auto ranges = derive_anchor_lease_ranges(kCandidateBase);
        chk("happy: derivation yields exactly the text RX span", ranges.size() == 1 &&
            ranges[0].b == RT_TEXT_B && ranges[0].e == RT_TEXT_E);
        bool lease_ok = true;
        try {
            std::vector<PLH::OwnedMemory::ReadRange> rr;
            for (const auto &r : ranges) rr.push_back({(const uint8_t *)(uintptr_t)r.b, (size_t)(r.e - r.b)});
            PLH::OwnedMemory::ReadLease rl({rr.data(), rr.size()});
        } catch (const std::exception &e) {
            lease_ok = false;
            printf("  lease error: %s\n", e.what());
        }
        chk("happy: REAL ReadLease over derived ranges PASSES", lease_ok);
        // r11 regression (kept in r14): a consumer range ending at the
        // UNALIGNED LOAD1 end is covered by outward quantization (the final
        // partial page [0x82ba000,0x82bc000) registered).
        const uint64_t ro_page_end = (RT_RO_E + 0x4000 - 1) & ~(uint64_t)0x3fff;
        bool r11_ok = true;
        try {
            std::vector<PLH::OwnedMemory::ReadRange> rr;
            rr.push_back({(const uint8_t *)(uintptr_t)RT_RO_B, (size_t)(ro_page_end - RT_RO_B)});
            PLH::OwnedMemory::ReadLease rl({rr.data(), rr.size()});
        } catch (...) { r11_ok = false; }
        chk("happy: lease over LOAD1 incl. final partial page PASSES (r11 kept)", r11_ok);
        // ...and the page after a registered span's end at a real hole (the
        // NOREAD tail) stays unregistered and refuses.
        bool hole_ok = false;
        try {
            std::vector<PLH::OwnedMemory::ReadRange> rr;
            rr.push_back({(const uint8_t *)(uintptr_t)RT_RW7_E, (size_t)0x4000});
            PLH::OwnedMemory::ReadLease rl({rr.data(), rr.size()});
        } catch (...) { hole_ok = true; }
        chk("happy: lease into the NOREAD tail THROWS (fail-closed)", hole_ok);
    }
#ifndef LEASE_TEST_OLD
    else if (!strcmp(scenario, "seedgate")) {
        // Dormant-throw hardening: the armed seed at 0x63d590 sits inside
        // exec-only LOAD0 pages that are NEVER registered. The gate must
        // refuse it loudly (bounded events naming the nearest span), and a
        // readable address must pass.
        build_phdr_fixture(4);
        shadow_all();
        drive_start(detail, sizeof detail);
        chk("seedgate: seed address NOT covered (dormant throw defused)",
            bridge_lease_span_containing(SEED_FNAME, 8) == nullptr);
        g_evs.clear();
        chk("seedgate: owned gate REFUSES the seed read", bridge_anchor_owned_covered(SEED_FNAME, 8) == false);
        chk("seedgate: refusal reported on the durable channel", ev_seen("bridge_anchor_refuse"));
        chk("seedgate: nearest-span report present", ev_seen("bridge_lease_near_lo"));
        g_evs.clear();
        chk("seedgate: readable LOAD1 address ADMITTED", bridge_anchor_owned_covered(0x7c26782, 8) == true);
        chk("seedgate: no refusal for covered read", !ev_seen("bridge_anchor_refuse"));
    }
    else if (!strcmp(scenario, "idempotent")) {
        // L5: re-registration of the same snapshot must never throw
        // "Owned regions must not overlap" (OwnedMemory.cpp:53-54).
        build_phdr_fixture(4);
        static lmf::OsVmMapping mapped[8];
        // parse the fixture into OsVmMapping form the way query_vmentry fills it
        {
            const unsigned char *buf = g_vm_fixture.data();
            uint64_t off = 0, len = g_vm_fixture.size(); unsigned n = 0;
            while (off + 64 <= len && n < 8) {
                uint32_t ksz; memcpy(&ksz, buf + off, 4);
                memcpy(&mapped[n].start, buf + off + 8, 8);
                memcpy(&mapped[n].end, buf + off + 16, 8);
                memcpy(&mapped[n].protection, buf + off + 56, 4);
                ++n; off += ksz;
            }
            bridge_register_from_frozen_snapshot(mapped, n);
            const unsigned first = g_lease_span_count;
            bridge_register_from_frozen_snapshot(mapped, n);  // would overlap-throw pre-r14
            chk("idempotent: second registration is a no-op", g_lease_span_count == first);
            chk("idempotent: PLH entry count unchanged", g_image_region_count == first);
        }
    }
#endif
    else if (!strcmp(scenario, "drift")) {
        // THE LAUNCH-8 REPRODUCTION. T1: text exec-only (prot=4). T2: the SAME
        // record readable-exec (prot=5). OLD registers from T1 and leases from
        // T2 -> the production ReadLease throws at the first text byte. NEW
        // registers from the captured (T2) array itself -> covered.
        g_drift = true;
        build_phdr_fixture(4);  // T1 state; sysctl mock applies drift per call policy
        shadow_all();
        const int rc = drive_start(detail, sizeof detail);
        chk("drift: mocked start completes", rc == MRS_OK);
        const auto ranges = derive_anchor_lease_ranges(kCandidateBase);
        chk("drift: derivation yields the text range [0x400000,0x787c000)",
            ranges.size() == 1 && ranges[0].b == RT_TEXT_B && ranges[0].e == RT_TEXT_E);
        bool threw = false;
        char what[128] = {0};
        try {
            std::vector<PLH::OwnedMemory::ReadRange> rr;
            for (const auto &r : ranges) rr.push_back({(const uint8_t *)(uintptr_t)r.b, (size_t)(r.e - r.b)});
            PLH::OwnedMemory::ReadLease rl({rr.data(), rr.size()});
        } catch (const std::invalid_argument &e) {
            threw = true;
            snprintf(what, sizeof what, "%s", e.what());
        } catch (...) { threw = true; }
#ifdef LEASE_TEST_OLD
        chk("drift OLD: production ReadLease THROWS (launch-8 reproduced)", threw);
        chk("drift OLD: message == launch-8 detail",
            !strcmp(what, "Owned read range includes unregistered bytes"));
#else
        chk("drift NEW: production ReadLease PASSES (frozen-snapshot registration)", !threw);
        chk("drift NEW: no coverage gap raised", !ev_seen("bridge_lease_gap"));
        chk("drift NEW: text pages covered by the captured-state registration",
            bridge_lease_span_containing(RT_TEXT_B, RT_TEXT_E - RT_TEXT_B) != nullptr);
#endif
    }
    else if (!strcmp(scenario, "exectext")) {
        // Base record exec-only at capture (the launch-9 r13 terminal shape):
        // the derivation is EMPTY and the conversion refuses with the geometry
        // text — the bridge must NOT fabricate coverage or a gap failure.
        build_phdr_fixture(4);  // no drift: text stays prot=4 everywhere
        shadow_all();
        const int rc = drive_start(detail, sizeof detail);
        chk("exectext: mocked start completes", rc == MRS_OK);
        const auto ranges = derive_anchor_lease_ranges(kCandidateBase);
        chk("exectext: derivation is EMPTY (adapter geometry refusal upstream)",
            ranges.empty());
        chk("exectext: text pages NOT registered (never lie about exec)",
            last_ev("bridge_lease_reg") == 3);  // LOAD1/LOAD3/LOAD7frag only
#ifndef LEASE_TEST_OLD
        chk("exectext: gate did NOT fire on an empty derivable set", !ev_seen("bridge_lease_gap"));
        chk("exectext: registry excludes the exec text",
            bridge_lease_span_containing(SEED_FNAME, 8) == nullptr);
#endif
    }
    else if (!strcmp(scenario, "cap")) {
        // >128 fragments, kernel-realistic ASCENDING order: base record first
        // (registers before any cap), LOAD1 next (anchors verifiable), then
        // 130 fragments ABOVE the quantized text span so the 128-region cap
        // genuinely bites. The derivable lease (base-derived) stays covered;
        // saturation is observable without being fatal to leases.
        g_vm_fixture.clear();
        rec(g_vm_fixture, 160, RT_TEXT_B, RT_TEXT_E, 5, "/app0/eboot.bin");  // base FIRST (lowest)
        rec(g_vm_fixture, 144, RT_RO_B, RT_RO_E, 1);                         // LOAD1: anchors
        for (int i = 0; i < 130; ++i)
            rec(g_vm_fixture, 144, 0x8800000 + (uint64_t)i * 0x4000,
                0x8800000 + (uint64_t)i * 0x4000 + 0x4000, 1);               // fragments (extent < 0xc0602c8)
        shadow_all();
        const int rc = drive_start(detail, sizeof detail);
        const auto ranges = derive_anchor_lease_ranges(kCandidateBase);
        chk("cap: derivation yields the base range", ranges.size() == 1 && ranges[0].b == RT_TEXT_B);
#ifdef LEASE_TEST_OLD
        chk("cap OLD: start completes (mocked chain)", rc == MRS_OK);
        chk("cap OLD: saturation silent (reg==128, spans==132)",
            last_ev("bridge_lease_reg") == 128 && last_ev("bridge_lease_reg_spans") == 132);
        {
            std::vector<PLH::OwnedMemory::ReadRange> rr;
            for (const auto &r : ranges) rr.push_back({(const uint8_t *)(uintptr_t)r.b, (size_t)(r.e - r.b)});
            bool lease_ok = true;
            try { PLH::OwnedMemory::ReadLease rl({rr.data(), rr.size()}); } catch (...) { lease_ok = false; }
            chk("cap OLD: derivable lease passes (base span registered pre-cap)", lease_ok);
        }
#else
        chk("cap NEW: start completes (anchor spans registered before the cap bites)",
            rc == MRS_OK);
        chk("cap NEW: cap saturation VISIBLE", ev_seen("bridge_lease_cap"));
        chk("cap NEW: registry capped at exactly 128", g_image_region_count == 128);
        chk("cap NEW: derivable base range stayed covered (no gate gap)",
            !ev_seen("bridge_lease_gap"));
        {
            std::vector<PLH::OwnedMemory::ReadRange> rr;
            for (const auto &r : ranges) rr.push_back({(const uint8_t *)(uintptr_t)r.b, (size_t)(r.e - r.b)});
            bool lease_ok = true;
            try { PLH::OwnedMemory::ReadLease rl({rr.data(), rr.size()}); } catch (...) { lease_ok = false; }
            chk("cap NEW: derivable lease passes identically to OLD", lease_ok);
        }
#endif
    }
    else if (!strcmp(scenario, "capgate")) {
        // THE GATE-REFUSAL SHAPE (synthetic record order: the base-containing
        // record placed LAST so the 128-region cap bites BEFORE it registers;
        // a real ascending snapshot can never do this to an anchor range, but
        // any future registration failure must fail closed the same way).
        // OLD: silent skip, then the production lease throws. NEW: the L3
        // gate refuses the start BEFORE any lease, naming the gap.
        g_vm_fixture.clear();
        rec(g_vm_fixture, 144, RT_RO_B, RT_RO_E, 1);   // LOAD1 first: anchors verifiable
        for (int i = 0; i < 129; ++i)
            rec(g_vm_fixture, 144, 0x5000000 + (uint64_t)i * 0x4000,
                0x5000000 + (uint64_t)i * 0x4000 + 0x4000, 1);
        rec(g_vm_fixture, 160, RT_TEXT_B, RT_TEXT_E, 5, "/app0/eboot.bin");  // base record LAST
        shadow_all();
        const int rc = drive_start(detail, sizeof detail);
        const auto ranges = derive_anchor_lease_ranges(kCandidateBase);
        chk("capgate: derivation yields the base range", ranges.size() == 1 && ranges[0].b == RT_TEXT_B);
#ifdef LEASE_TEST_OLD
        chk("capgate OLD: start completes (mocked chain)", rc == MRS_OK);
        bool threw = false;
        char what[128] = {0};
        try {
            std::vector<PLH::OwnedMemory::ReadRange> rr;
            for (const auto &r : ranges) rr.push_back({(const uint8_t *)(uintptr_t)r.b, (size_t)(r.e - r.b)});
            PLH::OwnedMemory::ReadLease rl({rr.data(), rr.size()});
        } catch (const std::invalid_argument &e) { threw = true; snprintf(what, sizeof what, "%s", e.what()); }
        chk("capgate OLD: production lease THROWS when the anchor span misses registration", threw);
        chk("capgate OLD: message == launch-8 detail",
            !strcmp(what, "Owned read range includes unregistered bytes"));
#else
        chk("capgate NEW: start REFUSES before any lease (MRS_ERR_CONVERSION)",
            rc == MRS_ERR_CONVERSION);
        chk("capgate NEW: refusal detail names the gap",
            !strncmp(detail, "lease coverage gap:", 19));
        chk("capgate NEW: gap event on the durable channel", ev_seen("bridge_lease_gap"));
        chk("capgate NEW: requested range reported", ev_seen("bridge_lease_gap_req"));
#endif
    }
    else if (!strcmp(scenario, "gap")) {
        // A consumer attempting ONE whole-extent read across exec text, real
        // spans, the NOREAD tail (and everything between) must fail CLOSED
        // with the first uncovered byte named — never covered up by
        // registering arbitrary memory.
        build_phdr_fixture(4);
        shadow_all();
        drive_start(detail, sizeof detail);
        const uint64_t req_b = RT_TEXT_B, req_len = RT_TAIL_E - RT_TEXT_B;
#ifndef LEASE_TEST_OLD
        const LeaseSpan *cov = bridge_lease_span_containing(req_b, req_len);
        chk("gap: whole-extent read is NOT covered (refuses)", cov == nullptr);
        g_evs.clear();
        chk("gap: owned gate refuses with report", bridge_anchor_owned_covered(req_b, 8) == false);
        chk("gap: refusal events emitted", ev_seen("bridge_anchor_refuse") && ev_seen("bridge_lease_ref_lo"));
        chk("gap: registered interval reported", ev_seen("bridge_lease_near_lo"));
        chk("gap: registered set stays 3 (no arbitrary registrations)", g_lease_span_count == 3);
#else
        (void)req_b; (void)req_len;
        chk("gap OLD: whole-extent read semantics untested (no gate in r13)", true);
#endif
    }
    else {
        printf("FAIL unknown scenario %s\n", scenario);
        return 2;
    }

    printf("RESULT[%s]: %s (%d failures)\n", scenario, fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
}
