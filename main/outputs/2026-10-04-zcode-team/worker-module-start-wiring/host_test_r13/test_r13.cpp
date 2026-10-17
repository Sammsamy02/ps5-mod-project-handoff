// PRODUCTION test for bridge r13: compiles the ACTUAL runtime_start_bridge_r13.cpp
// into this TU (no replicas). Mocks only: sysctl (link-symbol override),
// diagnostic_event (capture), plus the SIGSEGV shadow layer for guest-address
// reads inside window_anchor_verify. The PLH OwnedMemory service is compiled
// from its REAL source so bridge_register_readable_ranges + ReadLease are the
// production implementation.
//
// r13 refresh (worker-bridge-r13-polish, 2026-10-04) — supersedes host_test_r8:
//   * r9-era source=3 own-module debris removed (rung deleted in r10).
//   * the vacuous bridge_kve_prefix check (r8 revision's `a || !b` shape that
//     could never fail) replaced with EXACT r9-scan event assertions.
//   * r12/r13-delta assertions added:
//       - P2-1: activation released on BOTH defensive early returns
//         (ALWAYS-TRANSFERRED contract; r12 leaked here),
//       - r12 configured-roots attach exercised end-to-end through the REAL
//         modbridge_runtime_start_with_activation with the conversion chain
//         mocked: opt-in flags seen by the conversion, PRX stays the
//         conversion input (never the exe path), facts carry configured
//         roots, bridge_cfg_gamedir/workdir events via the capture,
//       - P3-1: bridge_lease_reg_spans diagnostic + 128-cap saturation made
//         observable (128 registered / 129 spans),
//       - r11 page quantization proven through the REAL ReadLease service
//         (final partial page covered; the page after the quantized end not),
//       - P3-4: MRS_ERR_STOP_EXN == 5 regression (throwing stop mock).
//   * fixed section-5 ReadRange arithmetic: the old `{base, 0x9000000-base}`
//     underflowed (0x09000000 < base) producing a huge length; lengths are
//     now computed from the actual fixture span ends.
//   * sections 1-4 semantics byte-identical to the r8 lineage otherwise.
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <setjmp.h>
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
static void ev_reset() { g_evs.clear(); }

// ---- sysctl mock: fixture records with variable kve_structsize ----
static std::vector<unsigned char> g_vm_fixture;      // raw record bytes
static int g_sysctl_fail = 0;
extern "C" int sysctl(int *, u_int, void *oldp, size_t *oldlenp, void *, size_t) {
    if (g_sysctl_fail || oldp == nullptr || oldlenp == nullptr) { if (oldlenp) *oldlenp = 0; return -1; }
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
static sigjmp_buf g_jmp; static volatile int g_jmp_ok = 0;
static auto rslot(ucontext_t *u, unsigned n) { auto *ss = &u->uc_mcontext->__ss; return n == 29 ? &ss->__fp : n == 30 ? &ss->__lr : &ss->__x[n]; }
static void fh(int, siginfo_t *, void *ctx) {
    ucontext_t *u = (ucontext_t *)ctx;
    const uint64_t pc = u->uc_mcontext->__ss.__pc;
    const uint32_t inst = *(volatile uint32_t *)pc;
    uint64_t addr; bool form = false;
    if ((inst & 0xFFC00000u) == 0x39400000u) { addr = *rslot(u, (inst >> 5) & 31) + ((inst >> 10) & 0xFFF); form = true; }
    else if ((inst & 0xFFE00C00u) == 0x38400000u) {  // LDURB Wt,[Xn,#imm9] (unscaled)
        const int64_t imm9 = (int64_t)((inst >> 12) & 0x1FF);
        addr = *rslot(u, (inst >> 5) & 31) + (imm9 < 256 ? imm9 : imm9 - 512);
        form = true;
    }
    else if ((inst & 0xFFE00C00u) == 0x38600800u) {
        const unsigned opt = (inst >> 13) & 7, s = (inst >> 12) & 1;
        const uint64_t b = *rslot(u, (inst >> 5) & 31), o = *rslot(u, (inst >> 16) & 31);
        if (opt == 3 && !s) { addr = b + o; form = true; }
    }
    if (!form) {
        static _Atomic(int) once = 0;
        if (once++ < 3) fprintf(stderr, "UNDECODED inst=%08x pc=%llx\n", inst, (unsigned long long)pc);
    }
    if (form) { if (const SR *r = sfind(addr)) { const unsigned rt = inst & 31; if (rt < 31) *rslot(u, rt) = r->b[addr - r->s]; u->uc_mcontext->__ss.__pc = pc + 4; return; } }
    if (form) { static _Atomic(int) once2 = 0; if (once2++ < 3) fprintf(stderr, "MISS addr=%llx inst=%08x\n", (unsigned long long)addr, inst); }
    siglongjmp(g_jmp, 1);
}

// ---- host shims for PS5 kernel ABI names the production TU uses ----
#include <sys/sysctl.h>
#ifndef KERN_PROC_VMMAP
#define KERN_PROC_VMMAP 35  // PS5 value; the sysctl mock ignores mib anyway
#endif
struct kinfo_vmentry { char _host_size_shim[1160]; };  // sizeof() only; parsing is raw-byte
// r13 P3-2: the production static_assert below now also pins THIS shim size
// at host-compile time (sizeof(kinfo_vmentry) == kve_sizeof_ == 1160).

// ---- THE PRODUCTION BRIDGE (actual source) ----
#include "../runtime_start_bridge_r13.cpp"
#include <polyhook2/OwnedMemory.hpp>

// ============================================================================
// r12/r13 start-chain mocks (host only). The bridge TU references these
// symbols; the target resolves them from the composed runtime object, the
// host test DEFINES them here so modbridge_runtime_start_with_activation can
// be exercised end-to-end. They record observations, never reimplement
// production logic.
// ============================================================================
struct mfa_activation { int alive; };  // opaque to production; test-visible
static unsigned g_mfa_releases = 0;
extern "C" void mfa_release(mfa_activation *a) { if (a != nullptr) ++g_mfa_releases; }

void modbridge::native::ActivationDeleter::operator()(mfa_activation *activation) const noexcept
{
    mfa_release(activation);  // production deleter semantics
}

lmf::LiveModuleOsOps lmf::make_sce_live_module_os_ops(void *) noexcept
{
    return lmf::LiveModuleOsOps{};  // all ops unbound: the bridge wraps them
}

lmf::RetainedLiveModuleFactsSupplier::RetainedLiveModuleFactsSupplier(
    const lmf::LiveModuleOsOps &ops, lmf::LiveFirmwareProfile) noexcept
    : ops_(ops) {}  // borrowed reference retained; never dereferenced by the mock
lmf::RetainedLiveModuleFactsSupplier::~RetainedLiveModuleFactsSupplier() {}
lmf::Status lmf::RetainedLiveModuleFactsSupplier::capture(const char *) noexcept
{
    return lmf::Status::Ok;  // capture outcome recorded by callers via facts()
}
const lmf::LiveModuleFactsSnapshot &lmf::RetainedLiveModuleFactsSupplier::facts() const noexcept
{
    static lmf::LiveModuleFactsSnapshot snapshot{};
    return snapshot;
}

// conversion observations (r12 delta: opt-ins + conversion input)
static RC::Platform::LmfConversionOptions g_conv_options_seen{};
static std::filesystem::path g_conv_path_seen;
static bool g_conv_fail_once = false;
static bool g_conv_probe_used = false;

RC::Platform::LmfConversionResult RC::Platform::TryMakeNativeLoaderFactsFromSupplier(
    std::shared_ptr<const lmf::RetainedLiveModuleFactsSupplier>,
    const std::filesystem::path &module_file_path,
    const RC::Platform::LmfConversionOptions &options) noexcept
{
    g_conv_options_seen = options;
    g_conv_path_seen = module_file_path;
    g_conv_probe_used = true;
    RC::Platform::LmfConversionResult r{};
    if (g_conv_fail_once) { g_conv_fail_once = false; r.converted = false; r.error_message = "mock refusal"; return r; }
    r.converted = true;
    return r;  // facts{} default: empty game_executable_path (Resolution B)
}

// program-context observations (r12 delta: configured-roots attach)
static std::filesystem::path g_ctx_module_path, g_ctx_exepath, g_ctx_gamedir, g_ctx_workdir;
static bool g_ctx_probe_used = false;

RC::Platform::NativeProgramContext RC::Platform::MakeNativeProgramContext(RC::Platform::NativeLoaderFacts facts)
{
    g_ctx_module_path = facts.module_file_path;
    g_ctx_exepath = facts.game_executable_path;
    g_ctx_gamedir = facts.configured_game_directory;
    g_ctx_workdir = facts.configured_working_directory;
    g_ctx_probe_used = true;
    return RC::Platform::NativeProgramContext{};
}

// runtime-owner observations
static std::string g_start_title, g_start_modroot;
static modbridge::native::ActivatedMods g_start_activation{nullptr};  // held until test clears it
static bool g_stop_throw = false;

modbridge::native::RuntimeOwner::RuntimeOwner(std::unique_ptr<RuntimeBackend>,
                                              std::shared_ptr<RuntimeAckSink>) {}
modbridge::native::RuntimeOwner::~RuntimeOwner() {}

std::unique_ptr<modbridge::native::RuntimeOwner>
modbridge::native::StartActivatedRuntimeWithAck(RC::Platform::NativeProgramContext,
                                                ActivatedMods activation,
                                                std::string_view expected_title,
                                                const char *mod_root)
{
    g_start_title.assign(expected_title);
    g_start_modroot = mod_root != nullptr ? mod_root : "";
    g_start_activation = std::move(activation);  // consume; released when the test clears it
    return std::unique_ptr<RuntimeOwner>(new RuntimeOwner(nullptr, nullptr));
}

bool modbridge::native::StopRuntimeWithAck(std::unique_ptr<RuntimeOwner> &owner)
{
    if (g_stop_throw) throw std::runtime_error("mock stop exception");
    owner.reset();  // real contract: the stopped owner is destroyed by this call
    return true;
}

static int fails = 0;
static void chk(const char *n, bool ok) { printf("%s %s\n", ok ? "PASS" : "FAIL", n); if (!ok) fails++; }

// probe-rung mock ops (file scope: functions cannot be defined in block scope)
static int probe_silent(void *, char *out, size_t) { (void)out; return 0; }  // writes nothing
static int probe_writer(void *, char *out, size_t capacity) {
    if (out != nullptr && capacity > 2) { out[0] = 'h'; out[1] = 'i'; out[2] = '\0'; return 0; }
    return -1;
}

// production window bytes (from the retained image, C2-verified)
static const unsigned char kE[64] = {0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75};
static const unsigned char kD[19] = {'C','u','r','r','e','n','t',' ','C','h','a','n','g','e','l','i','s','t',0};
static const unsigned char kB[8] = {0x55,0,0x45,0,0x35,0,0,0};
static const unsigned char kC[32] = {0x42,0,0x75,0,0x69,0,0x6c,0,0x64,0,0x49,0,0x64,0,0x4f,0,0x76,0,0x65,0,0x72,0,0x72,0,0x69,0,0x64,0,0x65,0,0,0};

static void shadow_all(char flip = 0, int which = -1) {
    static unsigned char ebuf[64], dbuf[19], bbuf[8], cbuf[32], c2buf[32];
    memcpy(ebuf, kE, 64); memcpy(dbuf, kD, 19); memcpy(bbuf, kB, 8); memcpy(cbuf, kC, 32); memcpy(c2buf, kC, 32);
    if (which == 3) dbuf[0] ^= 0xff;
    shadow(0x81d2a40, ebuf, 64);   // E rt
    shadow(0x7f8845d, dbuf, 19);   // D rt
    shadow(0x7c26782, bbuf, 8);    // B rt
    shadow(0x79cffda, cbuf, 32);   // C site 1 rt
    shadow(0x7ac8e58, c2buf, 32);  // C2 rt = 0x400000+0x76C8E58  // C2 site 2 rt
}
static void maps_full() {
    // igf7 same-invocation mapspans reality: ONE readable span covers
    // B/C/C2/D/E; A (0xc05f250) lies outside it, uncovered as observed.
    g_vm_fixture.clear();
    rec(g_vm_fixture, 144, 0x400000, 0x787c000, 4);            // exec-only image span
    rec(g_vm_fixture, 144, 0x787c000, 0x82bc000, 1);           // readable: B,C,C2,D,E
}

int main() {
    struct sigaction sa {}; sa.sa_sigaction = fh; sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr); sigaction(SIGBUS, &sa, nullptr);
    g_jmp_ok = 1; if (sigsetjmp(g_jmp, 1)) { printf("FAIL unexpected fault\n"); return 2; }

    // ===== 1) kve_path: 144-byte record must REFUSE a full path (user case) =====
    maps_full();
    // first record path region gets the first 8 bytes of /app0/eboot.bin
    memcpy(g_vm_fixture.data() + 136, "/app0/eb", 8);
    lmf::OsVmMapping m[16]; size_t n = 0;
    int rc = bridge_query_vmentry(nullptr, m, 16, &n);
    chk("144B walk ok (rc=0 n=2)", rc == 0 && n == 2);
    chk("144B record REFUSES full path (valid=false)", g_observed_exe_path_valid == false);
    // r13 (review P3-5): EXACT r9-scan assertions replace the vacuous
    // `a == X || a != Y` bridge_kve_prefix check (that tag is never emitted).
    // "/app0" LE prefix = 2f 61 70 30 -> 0x3070612f; no 'eboot' byte run in an
    // 8-byte "/app0/eb" region.
    chk("kve distinct prefix logged (/app0)", last_ev("bridge_kve_distinct0") == 0x7070612fu);
    chk("kve eboot flag 0 for 8B fragment", last_ev("bridge_kve_eboot") == 0);

    // ===== 2) kve_path: correctly sized record (160B, 24-byte path region) =====
    g_vm_fixture.clear();
    rec(g_vm_fixture, 160, 0x400000, 0x787c000, 4, "/app0/eboot.bin");
    rec(g_vm_fixture, 144, 0x818c000, 0x100118000, 1);
    rc = bridge_query_vmentry(nullptr, m, 16, &n);
    chk("160B walk ok", rc == 0 && n == 2);
    chk("160B record ACCEPTS complete path", g_observed_exe_path_valid == true && !strcmp(g_observed_exe_path, "/app0/eboot.bin"));
    chk("kve eboot flag 1 for full mount name", last_ev("bridge_kve_eboot") == 1);

    // ===== 3) executable path ladder =====
    lmf::LiveModuleOsOps base{};  // executable_path = nullptr (sysctl dead)
    char buf[256];
    rc = bridge_executable_path(&base, buf, sizeof buf);
    chk("kernel-observed served (source=1)", rc == 0 && !strcmp(buf, "/app0/eboot.bin") && last_ev("bridge_exepath_source") == 1);
    rc = bridge_executable_path(&base, nullptr, 100);
    chk("null out refused", rc == -1);
    rc = bridge_executable_path(&base, buf, 0);
    chk("zero capacity refused", rc == -1);
    // no observed path, configured value does not exist on host -> refuse
    g_observed_exe_path_valid = false; g_observed_exe_path[0] = 0;
    rc = bridge_executable_path(&base, buf, sizeof buf);
    chk("host: all rungs absent -> refuses (source=0)", rc == -1 && last_ev("bridge_exepath_source") == 0);

    // r13 (review P3-3 + ladder probe rung): the base op probe is now read
    // from zero-initialized storage. (a) an op that returns 0 WITHOUT writing
    // must fall through honestly (defined behavior; UB before r13):
    {
        lmf::LiveModuleOsOps silent{};
        silent.executable_path = probe_silent;
        rc = bridge_executable_path(&silent, buf, sizeof buf);
        chk("probe-rung: silent op falls through to refuse (source=0)", rc == -1 && last_ev("bridge_exepath_source") == 0);
    }
    // (b) an op whose 8-byte probe succeeds must be re-queried for the real
    // call; served + labeled source=1 (kernel-sysctl route label):
    {
        lmf::LiveModuleOsOps writer{};
        writer.executable_path = probe_writer;
        rc = bridge_executable_path(&writer, buf, sizeof buf);
        chk("probe-rung: probe then real call served (source=1)", rc == 0 && !strcmp(buf, "hi") && last_ev("bridge_exepath_source") == 1);
    }

    // ===== 4) identity gate through the PRODUCTION table =====
    maps_full(); ev_reset();
    bridge_query_vmentry(nullptr, m, 16, &n);   // fills mappings (C/C2 covered by fixtures)
    shadow_all();
    chk("gate: B&&D&&E&&(C||C2) passes", window_anchor_verify() == true && last_ev("anchor_c_either") == 1);
    maps_full(); bridge_query_vmentry(nullptr, m, 16, &n); shadow_all(0, 3);
    chk("gate: D mismatch refuses", window_anchor_verify() == false);
    // C at neither site: mappings cover but bytes absent -> shadow only B/D/E
    maps_full(); bridge_query_vmentry(nullptr, m, 16, &n);
    { static unsigned char e2[64], d2[19], b2[8], cw1[32], cw2[32];
      memcpy(e2,kE,64); memcpy(d2,kD,19); memcpy(b2,kB,8);
      memset(cw1,0x5a,32); memset(cw2,0x5a,32);  // covered but WRONG bytes at both C sites
      shadow_reset(); shadow(0x81d2a40,e2,64); shadow(0x7f8845d,d2,19); shadow(0x7c26782,b2,8);
      shadow(0x79cffda,cw1,32); shadow(0x7ac8e58,cw2,32); }
    chk("gate: C at neither site refuses", window_anchor_verify() == false && last_ev("anchor_c_either") == 0);

    // ===== 5) r12/r13 START-PATH DELTAS through the REAL start function =====
    // The conversion chain is mocked above; everything between (adoption,
    // options, attach, diagnostics, owner wiring) is the production code.
    // Each start call gets a DISTINCT readable span: registrations are
    // process-lifetime and the real service refuses overlapping regions.
    {
        char detail[256];
        mfa_activation act{1};
        auto one_readable = [](uint64_t s, uint64_t e) {
            g_vm_fixture.clear();
            rec(g_vm_fixture, 144, s, e, 1);
        };

        // P2-1 (r13): null-argument refusal must STILL release the activation.
        unsigned before = g_mfa_releases;
        rc = modbridge_runtime_start_with_activation(nullptr, nullptr, nullptr, &act,
                                                     detail, sizeof detail);
        chk("P2-1: null args -> MRS_ERR_INTERNAL", rc == MRS_ERR_INTERNAL && !strcmp(detail, "bad arguments"));
        chk("P2-1: activation RELEASED on null-arg early return (ALWAYS-TRANSFERRED)",
            g_mfa_releases == before + 1);

        // Conversion refusal path: honest gate + release + lmf capability events.
        one_readable(0x6000000, 0x6001000);
        g_conv_fail_once = true;
        mfa_activation act2{1};
        before = g_mfa_releases;
        rc = modbridge_runtime_start_with_activation(
            "/data/plugins/modbridge_supplier_composition.prx", "/data/modbridge/roots/PPSA33286",
            "PPSA33286", &act2, detail, sizeof detail);
        chk("conversion refusal -> MRS_ERR_CONVERSION", rc == MRS_ERR_CONVERSION);
        chk("release on conversion-refusal path", g_mfa_releases == before + 1);
        chk("lmf capability events on refusal", ev_seen("lmf_vmentry_cap") && ev_seen("lmf_provider_code"));

        // Success path: r12 opt-ins + Resolution-B attach + cfg diagnostics.
        one_readable(0x6100000, 0x6101000);
        ev_reset();
        mfa_activation act3{1};
        before = g_mfa_releases;
        rc = modbridge_runtime_start_with_activation(
            "/data/plugins/modbridge_supplier_composition.prx", "/data/modbridge/roots/PPSA33286",
            "PPSA33286", &act3, detail, sizeof detail);
        chk("start returns MRS_OK with empty detail", rc == MRS_OK && detail[0] == '\0');
        chk("conversion saw vm-scan opt-in", g_conv_probe_used && g_conv_options_seen.allow_vm_scan_base_as_module_image_base);
        chk("conversion saw configured-roots opt-in (r12)", g_conv_options_seen.allow_configured_roots_for_missing_executable_path);
        chk("conversion input stays the PRX path", g_conv_path_seen == "/data/plugins/modbridge_supplier_composition.prx");
        chk("facts carry EMPTY game_executable_path (never fabricated/PRX)", g_ctx_probe_used && g_ctx_exepath.empty());
        chk("facts carry configured_game_directory == /app0 (r12 attach)", g_ctx_gamedir == "/app0");
        chk("facts carry configured_working_directory == mod_root (r12 attach)", g_ctx_workdir == "/data/modbridge/roots/PPSA33286");
        chk("bridge_cfg_gamedir/workdir emitted (diagnostic capture)",
            last_ev("bridge_cfg_gamedir") == 1 && last_ev("bridge_cfg_workdir") == 1);
        chk("ack wiring got exact title + mod_root", g_start_title == "PPSA33286" && g_start_modroot == "/data/modbridge/roots/PPSA33286");
        chk("activation consumed exactly once by the chain", g_mfa_releases == before);
        g_start_activation.reset();  // mock-held activation -> release now
        chk("consumed activation released by its holder", g_mfa_releases == before + 1);

        // P2-1 (r13): already-started refusal must STILL release the activation.
        mfa_activation act4{1};
        before = g_mfa_releases;
        rc = modbridge_runtime_start_with_activation(
            "/data/plugins/modbridge_supplier_composition.prx", "/data/modbridge/roots/PPSA33286",
            "PPSA33286", &act4, detail, sizeof detail);
        chk("already-started -> MRS_ERR_INTERNAL", rc == MRS_ERR_INTERNAL && !strcmp(detail, "runtime already started"));
        chk("P2-1: activation RELEASED on already-started early return", g_mfa_releases == before + 1);

        // Stop paths. Normal: owner joined, slot emptied.
        chk("stop with owner returns 0", modbridge_runtime_stop() == 0);
        // Exception-escaped stop (renumber regression): catch -> reset + code 5.
        chk("MRS_ERR_STOP_EXN renumbered to 5 (r13 P3-4)", MRS_ERR_STOP_EXN == 5 && MRS_ERR_STOP_EXN != MRS_ERR_CONTEXT);
        one_readable(0x6300000, 0x6301000);
        rc = modbridge_runtime_start_with_activation(
            "/data/plugins/modbridge_supplier_composition.prx", "/data/modbridge/roots/PPSA33286",
            "PPSA33286", nullptr, detail, sizeof detail);  // null activation: consumed by prepare mock chain
        chk("restart for stop test", rc == MRS_OK);
        g_stop_throw = true;
        rc = modbridge_runtime_stop();
        g_stop_throw = false;
        chk("throwing stop -> MRS_ERR_STOP_EXN (5)", rc == MRS_ERR_STOP_EXN);
        chk("owner slot emptied by the throw path", modbridge_runtime_stop() == 0);
    }

    if (getenv("RUN_LEASE_SECTION")) {
    // ===== 6) lease registration with the REAL OwnedMemory service =====
    // NOTE: a SIGSEGV longjmp out of mutex-holding service code deadlocks the
    // host service singleton; run in a fresh process via RUN_LEASE_SECTION=1.
    // r13: fixture end is deliberately UNALIGNED (0xc000001 & 0x3fff != 0)
    // so page quantization is behaviorally proven, the span lies INSIDE the
    // anchored extent [0x400000, 0xc0602c8) (the r8-era 0x8018c000 span sat
    // ABOVE the extent end and could never register — part of why lease host
    // validation stayed PENDING), and ReadRange lengths are computed from the
    // real fixture ends (r8-era arithmetic underflowed).
    g_vm_fixture.clear();
    rec(g_vm_fixture, 144, 0x400000, 0x787c000, 4);           // exec: NOT registered
    rec(g_vm_fixture, 144, 0x8000000, 0xc000001, 1);          // readable, unaligned end
    ev_reset();
    bridge_register_readable_ranges();
    chk("lease reg: 1 region registered", g_image_region_count == 1);
    if (g_image_region_count != 1)
        printf("  DEBUG lease: count=%u reg=%08x spans=%u stop=%u entries=%u\n",
               g_image_region_count, last_ev("bridge_lease_reg"), last_ev("bridge_lease_reg_spans"),
               last_ev("bridge_vm_walk_stop"), last_ev("bridge_vmentry_entries"));
    chk("lease spans diagnostic == registered (P3-1)",
        last_ev("bridge_lease_reg_spans") == 1 && last_ev("bridge_lease_reg") == 1);
    // exact-byte consumer range must PASS
    PLH::OwnedMemory::ReadRange rr[] = { {(const uint8_t*)0x8000000, (size_t)(0xc000001 - 0x8000000)} };
    bool lease_ok = true;
    try { PLH::OwnedMemory::ReadLease rl({rr, 1}); } catch (...) { lease_ok = false; }
    chk("ReadLease over exact registered bytes PASSES", lease_ok);
    // r11 regression: the FINAL PARTIAL PAGE (here [0xc000000, 0xc004000))
    // must be covered by the round-outward quantization.
    PLH::OwnedMemory::ReadRange rq[] = { {(const uint8_t*)0xc000000, (size_t)(0xc004000 - 0xc000000)} };
    bool quantized_ok = true;
    try { PLH::OwnedMemory::ReadLease rl({rq, 1}); } catch (...) { quantized_ok = false; }
    chk("ReadLease over final partial page PASSES (round-outward quantization)", quantized_ok);
    // ...and the page AFTER the quantized end must stay unregistered.
    PLH::OwnedMemory::ReadRange rb[] = { {(const uint8_t*)0xc004000, 0x4000} };
    bool beyond_refused = false;
    try { PLH::OwnedMemory::ReadLease rl({rb, 1}); } catch (...) { beyond_refused = true; }
    chk("ReadLease beyond quantized end THROWS", beyond_refused);

    // P3-1: cap saturation is now OBSERVABLE. 129 distinct readable pages
    // inside the extent: 128 registered, 129 counted.
    g_vm_fixture.clear();
    rec(g_vm_fixture, 144, 0x400000, 0x787c000, 4);
    for (int i = 0; i < 129; ++i)
        rec(g_vm_fixture, 144, 0x5000000 + (uint64_t)i * 0x4000,
            0x5000000 + (uint64_t)i * 0x4000 + 1, 1);
    ev_reset();
    bridge_register_readable_ranges();
    chk("lease cap: exactly 128 registered (no overflow)",
        g_image_region_count == 128 && last_ev("bridge_lease_reg") == 128);
    chk("lease cap saturation VISIBLE: 129 spans vs 128 reg (P3-1)",
        last_ev("bridge_lease_reg_spans") == 129);
    }  // RUN_LEASE_SECTION

    printf("RESULT: %s (%d failures)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
}
