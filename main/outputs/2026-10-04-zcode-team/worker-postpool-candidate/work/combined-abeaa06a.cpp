// runtime_start_bridge.cpp — the module_start -> composed-runtime wiring.
//
// r14 (2026-10-05, worker-lease-coverage-fix): r13 + the lease-coverage
// hardening. ROOT CAUSE (launch-8 pid 234 code=2 "Owned read range includes
// unregistered bytes", OwnedMemory.cpp:122): the r10/r13 flow registered
// readable spans from a FRESH sysctl taken BEFORE supplier->capture(), while
// the ReadLease ranges are derived from the CAPTURE's frozen snapshot
// (anchor_mappings, live_module_facts.cpp:449-457, through
// derive_readable_ranges, lmf_to_native_loader_facts.cpp:77-109). Two
// different snapshots = a race the r11 page-quantization could never close:
// when the base-containing record (the only record containing chosen_base
// 0x400000) is exec-only at T1 but readable at T2, the lease walks text pages
// that were deliberately never registered. r14 changes WHO registers and WHEN,
// not what may be registered:
//   L1  Registration moves INTO the scan_image_base wrapper — the FIRST op
//       capture() drives with the frozen mappings array — so the registered
//       set is derived from the SAME snapshot bytes the conversion later
//       leases. No extra sysctl, no drift class. Pre-capture registration
//       (r13 bridge_register_readable_ranges) is REMOVED.
//   L2  True-protection registration: readable records register R (|X when
//       the kve bits carry exec). Never W (the service must not authorize
//       writes into game memory). Exec-only and no-protection records stay
//       unregistered — the fail-closed direction.
//   L3  Coverage gate: after capture, the exact ranges the conversion WILL
//       lease (records containing the chosen base, readable, clamped to the
//       extent — byte-identical derivation to the adapter) are checked
//       against the registry. A gap refuses the start LOUDLY BEFORE any
//       lease (MRS_ERR_CONVERSION + bounded detail naming the gap and the
//       nearest registered interval) instead of a deep PLH throw.
//   L4  The window-anchor byte reads are gated on the registry: a fresh
//       VMMAP coverage hit that is not owned-registered refuses with
//       diagnostics instead of dereferencing (A stays logged-not-required).
//   L5  Idempotence: skip-if-covered, so a re-entrant start can never throw
//       "Owned regions must not overlap" from duplicate registrations.
// Cap semantics unchanged (128) but saturation now refuses loudly via the
// same gate. bridge_lease_reg_spans / bridge_lease_reg tags are kept with
// the frozen-snapshot basis. Launch-9 attribution note: the shipped r13
// passed registration (spans=6/reg=6, pid 273 delta) and refused later at
// conversion geometry; the code=2 lease throw belonged to launch-8's r10
// binary (pid 234). r14 is hardening for the scan-read stage (armed seed
// verification reads), not a regression fix for r13's terminal state.
//
// worker-facts-geometry-fix STAGED COPY (2026-10-05): canonical
// worker-lease-coverage-fix/runtime_start_bridge_r14.cpp (conversion
// behavior byte-identical) + the controller-directed geometry diagnostics
// (lmf_extent_base_low/high, lmf_extent_end_low/high, lmf_anchor_count,
// lmf_intervals_found) emitted on BOTH the conversion-refusal and the
// conversion-success paths through the EXISTING diagnostic channel. No new
// sink, no new imports, no ordering change: configured roots still attach
// strictly after a successful conversion per Resolution B 2.5/2.7 (the
// adapter consumes the opt-in FLAG, never the root fields — verified: the
// canonical adapter TU references neither configured_* field).
//
// r13 (2026-10-04, worker-bridge-r13-polish): r12 + the GO-WITH-FIXES review
// diffs, all five applied (worker-bridge-r12-final-review/REVIEW-VERDICT.md):
//   P2-1  activation adopted FIRST — before the defensive early returns — so
//         the header's ALWAYS-TRANSFERRED ownership contract holds on every
//         exit path (contract hole was unreachable from the shipped entry).
//   P3-1  bridge_lease_reg_spans diagnostic: total readable-intersecting
//         spans vs the 128-region cap (saturation is now observable).
//   P3-2  static_assert pins kve_sizeof_ to sizeof(kinfo_vmentry).
//   P3-3  ladder probe[8] zero-initialized.
//   P3-4  MRS_ERR_STOP_EXN renumbered 2->5 in runtime_start_bridge.h (was
//         colliding with MRS_ERR_CONTEXT; entry reads only MRS_OK /
//         MRS_ERR_START / nonzero — no consumer change needed).
// Otherwise byte-identical in behavior to r12 (sha feed5254...); all anchors,
// walk bounds, ladder semantics, and configured-roots attach unchanged.
//
// Consumed interfaces (all DEFINED in the composition-4 composed object,
// verified against ue4ss-native-supplier-composition-4/run-3/runtime.map,
// runtime sha256 45dd9901db7d626e63b8ac813bd8bf09f00938b1c5ecfa5f726ff7c9ff075af5):
//   _ZN3lmf27make_sce_live_module_os_opsEPv
//   _ZN3lmf31RetainedLiveModuleFactsSupplier...  (capture/validate/release)
//   _ZN2RC8Platform36TryMakeNativeLoaderFactsFromSupplierENSt3__110shared_ptrIKN3lmf31RetainedLiveModuleFactsSupplierEEERKNS1_4__fs10filesystem4pathERKNS0_20LmfConversionOptionsE
//   _ZN2RC8Platform24MakeNativeProgramContextENS0_17NativeLoaderFactsE
//   _ZN9modbridge6native28StartActivatedRuntimeWithAckEN2RC8Platform20NativeProgramContextENSt3__110unique_ptrI14mfa_activationNS0_17ActivationDeleterEEENS4_17basic_string_viewIcNS4_11char_traitsIcEEEEPKc
//   _ZN9modbridge6native19StopRuntimeWithAckERNS_10unique_ptrINS0_12RuntimeOwnerENS1_11default_deleteIS3_EEEE
//
// Signatures are taken from the ACTUAL composed-unit headers
// (ue4ss-native-tick-file-owner-1/include + worker-query-s2-arm
// integration-patch new-units), which are the same files the composition
// compiled these units from (run-3 commands.json argv template).

// LAUNCH-10 CONSOLIDATED BRIDGE (controller 2026-10-05): r14 (lease two-snapshot race fix, L1-L5)
// + geometrydiag lmf_* conversion diagnostics (3 hunks, clean-merged). Span-publish accessors
// (pool-bound fix) apply on top as hunks when delivered. Originals untouched.
// SPANPUB MERGED onto r14+diag by controller: publish call relocated into the
// scan wrapper beside the L1 frozen-snapshot registration (the r13 anchoring site
// no longer exists in r14); accessors at file scope. Same-capture preserved.
#include "runtime_start_bridge.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <new>
#include <string_view>
#include <utility>

#include "activated_mod_source.hpp"
#include "live_module_facts.hpp"
// r2 (controller revision, 2026-10-04): the complete LiveModuleOsOps definition
#include "live_module_os_ops.hpp"
#include "lmf_to_native_loader_facts.hpp"
#include "NativeLoaderContext.hpp"
#include "runtime_ack_supplier.hpp"
#include "runtime_owner.hpp"
#include "ue4ss_backend.hpp"

// r4: bridge-side VMMAP query with a larger buffer (launch-2 evidence: the
// composition's 1408-entry buffer gets ENOMEM with a save loaded).
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/syscall.h> // SYS_mincore (kernel-header-sourced number, never hand-typed)
#include <sys/user.h>
#include <unistd.h>
#include <errno.h>

// gameid (worker-gameid-unit staged NEXT candidate): NID-routed libc IO for the
// identity self-report. fopen/fread are IN the frozen import contract
// (import-contract.json required_strong set; "sysctl, fopen/fwrite — proven
// live" per the session-934 note below); <fcntl.h> flags are header-only
// constants; <time.h> clock_gettime is in the frozen contract (the diag sink
// already calls it). errno is a MACRO in the SDK errno.h — NEVER ::errno
// (edit-11 lesson, packaging selftest). NO raw syscalls anywhere in this
// section: they are process-fatal in-game (session 934, edit 13b).
#include <cstdio>
#include <fcntl.h>
#include <time.h>

// Bootstrap-unit channel (diagnostic.c, linked into the same PRX): used for
// post-failure capability visibility; no new imports (same TU set).
extern "C" void diagnostic_event(const char *tag, uint32_t value);

// Declared only in live_module_os_ops_sce.cpp (no header); mangled symbol
// verified present in the composed runtime.map (see file top).
namespace lmf
{
struct LiveModuleOsOps;
LiveModuleOsOps make_sce_live_module_os_ops(void *context) noexcept;
}

// r14 (L4): owned-coverage gate for the window-anchor reads. Defined at file
// scope in the r14 registration block below; forward-declared here because
// window_anchor_verify (anonymous namespace, below) is its first caller.
static bool bridge_anchor_owned_covered(uint64_t addr, uint32_t len) noexcept;


namespace
{
// The one process-lifetime owner. module_start/module_stop are serialized by
// runtime_module_entry.c's entry_guard, so no additional locking is required.
auto owner_slot() -> std::unique_ptr<modbridge::native::RuntimeOwner>&
{
    static std::unique_ptr<modbridge::native::RuntimeOwner> owner;
    return owner;
}

// Bounded printable-ASCII copy: control bytes and non-ASCII become '?', the
// result is always NUL-terminated (detail may be null / capacity 0).
void copy_detail(char *detail, unsigned detail_capacity, const char *text) noexcept
{
    if (detail == nullptr || detail_capacity == 0u)
    {
        return;
    }
    unsigned used = 0u;
    if (text != nullptr)
    {
        while (*text != '\0' && used + 1u < detail_capacity)
        {
            const char c = *text++;
            detail[used++] = (c >= 0x20 && c <= 0x7e) ? c : '?';
        }
    }
    detail[used] = '\0';
}

void copy_detail_or_empty(char *detail, unsigned detail_capacity) noexcept
{
    copy_detail(detail, detail_capacity, "");
}

// Firmware profile per worker-1-live-supplier REPORT "Typed contract for the
// controller": {352, 4, fw=0 (unknown, recorded honestly),
// bridge_flag=false (unproven handle-domain equality), "eboot.bin"}.
auto prospero_live_firmware_profile() noexcept -> lmf::LiveFirmwareProfile
{
    lmf::LiveFirmwareProfile profile{};
    profile.module_info_abi_size = 352u;
    profile.module_segment_capacity = 4u;
    profile.fw_version = 0u;  // unknown: never guessed here
    profile.allow_loader_handle_as_module_handle = false;
    const char *suffix = "eboot.bin";
    unsigned i = 0u;
    for (; suffix[i] != '\0' && i + 1u < sizeof(profile.exe_name_suffix); ++i)
    {
        profile.exe_name_suffix[i] = suffix[i];
    }
    profile.exe_name_suffix[i] = '\0';
    return profile;
}

// ---- validated data-window anchor (r3) ------------------------------------
// The eboot header page is EXEC-ONLY on this target (VMMAP [0x400000,
// 0x787c000) prot=4, trusted in-game records), so the in-memory ELF header
// scan can never anchor the main image (composition-5 launch 2026-10-04:
// NoImageAnchor/needs-base). These windows were verified IN-GAME (igf7
// 4/4 candidates, base 0x400000 single-load-bias) and are unique in the
// retained eboot image (sha 3b6cfad4...): verify BOTH inside readable VMMAP
// spans before trusting the base. No new imports: byte-loop compare only.
struct DataWindowAnchor {
    uint64_t vaddr;      // eboot p_vaddr of the window
    uint32_t length;
    const unsigned char *bytes;
    bool required;       // r7 (user directive): the established B/D/E
                         // correspondence together is the identity gate.
    const char *id;
};
static const unsigned char kWindowB[] = { 0x55, 0x00, 0x45, 0x00, 0x35, 0x00, 0x00, 0x00 };  // L"UE5" (igf7-verified, unique)
static const unsigned char kWindowC[] = { 0x42, 0x00, 0x75, 0x00, 0x69, 0x00, 0x6c, 0x00, 0x64, 0x00, 0x49, 0x00, 0x64, 0x00, 0x4f, 0x00, 0x76, 0x00, 0x65, 0x00, 0x72, 0x00, 0x72, 0x00, 0x69, 0x00, 0x64, 0x00, 0x65, 0x00, 0x00, 0x00 };  // BuildIdOverride UTF-16 (2 legitimate occurrences)
static const unsigned char kWindowD[] = { 0x43, 0x75, 0x72, 0x72, 0x65, 0x6e, 0x74, 0x20, 0x43, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x6c, 0x69, 0x73, 0x74, 0x00 };  // Current Changelist (igf7-verified, unique)
static const unsigned char kWindowE[] = { 0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75 };  // AES S-box head (igf7-verified, unique)
static const unsigned char kWindowA[] = { 0x04, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x47, 0x4e, 0x55, 0x00, 0x04, 0xbb, 0x43, 0x30, 0xa7, 0xd7, 0x1d, 0xce, 0xfe, 0x1c, 0xc2, 0xc4, 0xd1, 0x97, 0x69, 0xd5, 0x00, 0x00, 0x00, 0x00 };  // build-id note (logged only; observed uncovered in-game)
static const DataWindowAnchor kDataWindowAnchors[] = {
    { 0x7dd2a40, 64, kWindowE, true, "E" },
    { 0x7b8845d, 19, kWindowD, true, "D" },
    { 0x7826782, 8, kWindowB, true, "B" },
    { 0x75cffda, 32, kWindowC, false, "C" },   // required via either-site clause (r8)
    { 0x76c8e58, 32, kWindowC, false, "C2" },  // required via either-site clause (r8)
    { 0xbc5f250, 36, kWindowA, false, "A" },
};
static constexpr size_t kAnchorCount = sizeof(kDataWindowAnchors) / sizeof(kDataWindowAnchors[0]);
static constexpr uint64_t kCandidateBase = 0x400000;       // validated bias
// Resolution B (user directive 2026-10-04): game_executable_path is NEVER
// satisfied by our own PRX. When no real path is observable, the adapter
// converts with it EMPTY and these explicitly CONFIGURED roots carry the
// directory derivations. kConfiguredGameDirectory is a title-mount
// configuration value, never a kernel-observed path; the working root is the
// entry's mod_root parameter.
static const char *kConfiguredGameDirectory = "/app0";
static constexpr uint64_t kImageExtentVaddr = 0xbc602c8;  // max(p_vaddr+p_memsz)

// VMMAP protection bits (kinfo_vmentry kve_protection, observed in-game:
// 1=read, 4=exec, 3=rw). Matches lmf base-scan readable-bit semantics.
static constexpr uint32_t kVmProtRead = 1u;

// ---- r4: enlarged VMMAP snapshot (bridge-owned .bss) ----------------------
// Launch-2 (2026-10-04, save loaded): the composition's 1408-entry sysctl
// buffer returns ENOMEM (vmentry_cap=Failed), killing BOTH the core scan and
// the window anchor. 16384 entries (~19 MiB, zero-fill .bss) covers the
// in-save mapping count with wide margin; count is exported to the
// diagnostic channel for evidence.
static constexpr size_t kBridgeVmmapEntries = 32768;
static kinfo_vmentry g_bridge_vmmap[kBridgeVmmapEntries];  // r5: walked as raw bytes

// r5: per-record kve_structsize walk (port of the PROVEN in-game walker,
// worker-c2-probe/src/vmmap_walk.c mode 1, host-tested lineage). Launch-3
// evidence: kernel VMMAP records are VARIABLE size; the fixed 1160-byte
// stride parse (both the composition unit and bridge r4) yields len%1160!=0
// -> refusal. Field offsets per the pinned SDK header: structsize@0,
// start@8, end@16, protection@56. Sanity bounds identical to the proven
// walk: kve_structsize in [64, sizeof(kinfo_vmentry)], 4-aligned, record
// must fit the buffer; on an insane size STOP, never guess.
static constexpr uint32_t kKveHdr = 64;   // pre-path header bytes
static constexpr uint32_t kve_sizeof_ = 1160;  // pinned SDK sizeof(kinfo_vmentry)
// r13 (review P3-2): pin the duplicated constant to the type so a future SDK
// refresh cannot silently desynchronize the r7/r9 walkers (which use
// kve_sizeof_) from the r6 walker (which uses sizeof(kinfo_vmentry)).
static_assert(sizeof(kinfo_vmentry) == kve_sizeof_,
              "kve_sizeof_ must equal the pinned SDK sizeof(kinfo_vmentry)");

static uint32_t ld32b(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t ld64b(const unsigned char *p)
{
    return (uint64_t)ld32b(p) | ((uint64_t)ld32b(p + 4) << 32);
}

// r6: pure per-record walk (host-testable; no OS calls). Semantics:
//   stop_reason 0 = clean end (off == len exactly)
//   stop_reason 1 = malformed: insane kve_structsize (< 64, > 1160,
//                   misaligned, or record overruns the buffer with a sane
//                   header) -> caller FAILS the query
//   stop_reason 2 = truncated: partial tail record OR trailing fragment
//                   shorter than the 64-byte header -> caller succeeds with
//                   a lower-bound total (the consumer records truncation)
static void bridge_vmmap_walk(const unsigned char *buf, uint64_t len,
                              lmf::OsVmMapping *out, size_t capacity,
                              size_t *entries_out, uint32_t *stop_reason)
{
    const uint32_t kve_sizeof = static_cast<uint32_t>(sizeof(kinfo_vmentry));
    size_t entries = 0;
    uint32_t stop = 0;
    uint64_t off = 0;
    while (off + kKveHdr <= len)
    {
        const uint32_t ksz = ld32b(buf + off);
        if (ksz < kKveHdr || ksz > kve_sizeof || (ksz & 3u) != 0)
        { stop = 1; break; }                        // malformed: never guess
        if (off + ksz > len)
        { stop = (ksz <= kve_sizeof) ? 2u : 1u; break; }  // sane hdr, cut body = truncation
        const uint64_t s = ld64b(buf + off + 8);
        const uint64_t e = ld64b(buf + off + 16);
        const uint32_t prot = ld32b(buf + off + 56);
        if (out != nullptr && entries < capacity)
        {
            out[entries].start = s;
            out[entries].end = e;
            out[entries].protection = prot;
        }
        ++entries;
        off += ksz;
    }
    if (stop == 0 && off != len)
        stop = 2;  // trailing fragment shorter than a record header
    *entries_out = entries;
    *stop_reason = stop;
}

// r7: kernel-observed executable path. The conversion requires a non-empty
// executable_path and refuses fabrication (launch-5: GameExecutablePathMissing;
// sysctl KERN_PROC_PATHNAME returns ENOENT in-game, observed E2). The kernel's
// own VMMAP record for the anchored exec span carries kve_path (offset 136,
// bounded by kve_structsize) naming the backing file — an observation, not a
// fabrication. Captured during the walk; served via the ops wrapper.
static char g_observed_exe_path[1040];
static bool g_observed_exe_path_valid = false;

static int bridge_query_vmentry(void *, lmf::OsVmMapping *out, size_t capacity,
                                size_t *count)
{
    if (count == nullptr) return -1;
    *count = 0;
    // r8 (user correction): cached path validity is PER-CAPTURE state; clear
    // it on every query so a stale kernel-observed path never survives into a
    // later capture. The supplier's capture pass calls query_vmentry BEFORE
    // executable_path (pid -> VMMAP -> executable_path), so this ordering
    // guarantees the wrapper only ever serves same-capture state.
    g_observed_exe_path[0] = '\0';
    g_observed_exe_path_valid = false;
    const long pid = ::getpid();
    if (pid < 0) return -2;
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, static_cast<int>(pid)};
    size_t len = sizeof(g_bridge_vmmap);
    if (::sysctl(mib, 4, g_bridge_vmmap, &len, nullptr, 0) != 0) return -3;
    if (len < kKveHdr) return -4;
    const unsigned char *buf = reinterpret_cast<const unsigned char *>(g_bridge_vmmap);
    size_t entries = 0;
    uint32_t stop_reason = 0;
    bridge_vmmap_walk(buf, len, out, capacity, &entries, &stop_reason);
    ::diagnostic_event("bridge_vmentry_entries", static_cast<uint32_t>(entries));
    ::diagnostic_event("bridge_vm_walk_stop", stop_reason);
    // r6 contract (user directive 2026-10-04): the lmf consumer explicitly
    // supports truncated snapshots (records vm_total > copied) and span-
    // validates every access, so truncation is a SUCCESS with an honest
    // lower-bound total; MALFORMATION (insane structsize) has no consumer
    // support and must surface as failure.
    if (stop_reason == 1) return -6;          // malformed record: fail loudly
    if (entries == 0) return -5;              // nothing parsed at all
    if (stop_reason == 2) { *count = entries + 1; return 0; }  // truncated: honest lower bound
    *count = entries;                         // clean end (off == len)
    // r7: capture kve_path of the record covering the anchored image base.
    {
        uint64_t off2 = 0;
        while (off2 + kKveHdr <= len) {
            const uint32_t ksz2 = ld32b(buf + off2);
            if (ksz2 < kKveHdr || ksz2 > kve_sizeof_ || (ksz2 & 3u) != 0 || off2 + ksz2 > len) break;
            const uint64_t s2 = ld64b(buf + off2 + 8), e2 = ld64b(buf + off2 + 16);
            if (s2 <= kCandidateBase && kCandidateBase < e2 && ksz2 > 136) {
                const unsigned char *pathb = buf + off2 + 136;
                const uint64_t path_cap = ksz2 - 136;
                uint64_t n = 0;
                while (n < path_cap && n < sizeof(g_observed_exe_path) - 1 && pathb[n] != 0) ++n;
                if (n > 0 && n < path_cap && pathb[n] == 0) {  // NUL-terminated in-record
                    for (uint64_t i = 0; i < n; ++i) g_observed_exe_path[i] = (char)pathb[i];
                    g_observed_exe_path[n] = '\0';
                    g_observed_exe_path_valid = g_observed_exe_path[0] == '/';
                }
                break;
            }
            off2 += ksz2;
        }
        // r9 discovery: the anchored exec record's path region was ZERO
        // (launch-7: sig 0) — the eboot mapping carries no path. Scan EVERY
        // record's kve_path region: log up to 4 distinct non-zero 8-byte
        // prefixes and flag any containing the bytes 'e','b','o','o','t' —
        // file-backed mappings reveal the in-process mount layout.
        {
            uint32_t distinct[4] = {0, 0, 0, 0}; unsigned nd = 0; uint32_t eboot_hit = 0;
            uint64_t off3 = 0;
            while (off3 + kKveHdr <= len) {
                const uint32_t ksz3 = ld32b(buf + off3);
                if (ksz3 < kKveHdr || ksz3 > kve_sizeof_ || (ksz3 & 3u) != 0 || off3 + ksz3 > len) break;
                if (ksz3 > 140) {
                    const unsigned char *pp = buf + off3 + 136;
                    const uint64_t cap3 = ksz3 - 136;
                    uint32_t pre = 0; bool nonzero = false;
                    for (int k = 0; k < 4 && (uint64_t)k < cap3; ++k) {
                        pre |= (uint32_t)pp[k] << (8 * k);
                        if (pp[k] != 0) nonzero = true;
                    }
                    for (uint64_t k = 0; k + 5 <= cap3; ++k)
                        if (pp[k]=='e'&&pp[k+1]=='b'&&pp[k+2]=='o'&&pp[k+3]=='o'&&pp[k+4]=='t')
                            { eboot_hit = 1; break; }
                    if (nonzero) {
                        bool seen = false;
                        for (unsigned q = 0; q < nd; ++q) if (distinct[q] == pre) seen = true;
                        if (!seen && nd < 4) distinct[nd++] = pre;
                    }
                }
                off3 += ksz3;
            }
            ::diagnostic_event("bridge_kve_distinct0", distinct[0]);
            ::diagnostic_event("bridge_kve_distinct1", distinct[1]);
            ::diagnostic_event("bridge_kve_distinct2", distinct[2]);
            ::diagnostic_event("bridge_kve_distinct3", distinct[3]);
            ::diagnostic_event("bridge_kve_eboot", eboot_hit);
        }
        uint32_t path_sig = 0;
        for (int i = 0; i < 4 && g_observed_exe_path[i]; ++i)
            path_sig |= (uint32_t)(unsigned char)g_observed_exe_path[i] << (8 * i);
        ::diagnostic_event("bridge_exepath_valid", g_observed_exe_path_valid ? 1u : 0u);
        ::diagnostic_event("bridge_exepath_sig", path_sig);
    }
    return 0;
}

static bool window_anchor_verify() {
    // Fetch fresh mappings via the SAME proven vmentry op the core uses.
    static lmf::OsVmMapping mappings[kBridgeVmmapEntries];
    size_t total = 0;
    if (bridge_query_vmentry(nullptr, mappings, kBridgeVmmapEntries, &total) != 0 ||
        total == 0)
        return false;
    const size_t count = total < kBridgeVmmapEntries ? total : kBridgeVmmapEntries;
    // r7 (user directive 2026-10-04): E alone must NOT authorize identity.
    // The established B/C/D/E correspondence at their predetermined image
    // offsets, together with mapping coverage, is the identity gate. A stays
    // explicitly refused-but-logged; its failure never drops the others.
    // C accepts a match at EITHER known occurrence site (legitimate repeat).
    bool ok[kAnchorCount];
    for (size_t w = 0; w < kAnchorCount; ++w) ok[w] = false;
    for (size_t w = 0; w < kAnchorCount; ++w) {
        const DataWindowAnchor &a = kDataWindowAnchors[w];
        const uint64_t rt = kCandidateBase + a.vaddr;
        bool covered = false;
        for (size_t i = 0; i < count; ++i) {
            if ((mappings[i].protection & kVmProtRead) != 0 &&
                mappings[i].start <= rt && rt + a.length <= mappings[i].end) {
                covered = true; break;
            }
        }
        ::diagnostic_event("anchor_cov", (uint32_t)((w << 16) | (covered ? 1 : 0)));
        if (!covered) continue;
        // r14 (L4): the fresh-VMMAP coverage hit is necessary but not
        // sufficient — the byte read below is admitted only when the FROZEN
        // owned registry covers it too. Snapshot drift between this query and
        // the capture array refuses loudly here (bounded events, window id in
        // the tag) instead of dereferencing unowned bytes.
        if (!bridge_anchor_owned_covered(rt, a.length)) {
            ::diagnostic_event("anchor_owned_ref", (uint32_t)(w << 16));
            continue;
        }
        const unsigned char *p = reinterpret_cast<const unsigned char *>(rt);
        bool matched = true; uint32_t mm = 0xffffffffu;
        for (uint32_t i = 0; i < a.length; ++i)
            if (p[i] != a.bytes[i]) { matched = false; mm = i; break; }
        ::diagnostic_event("anchor_match", (uint32_t)((w << 16) | (matched ? 1 : 0)));
        if (!matched) { ::diagnostic_event("anchor_mm", (uint32_t)((w << 16) | mm)); continue; }
        ok[w] = true;
    }
    // r8 (user correction): the ACTUAL correspondence requirement is
    // B && D && E && (C || C2) — the justified occurrence rule for C's
    // legitimate repeat. C participates through the either-site clause,
    // not as an optional extra.
    const bool c_correspondence = ok[3] || ok[4];
    ::diagnostic_event("anchor_c_either", c_correspondence ? 1u : 0u);
    return ok[0] && ok[1] && ok[2] && c_correspondence;  // E, D, B required + C either-site
}


}  // namespace

// r8: named executable_path op. Candidate ladder, honest and labeled:
//   1. complete kernel-observed kve_path (NUL-terminated within its record;
//      144-byte records cannot carry a full path, so this usually refuses)
//   2. the base sysctl probe (KERN_PROC_PATHNAME; observed ENOENT in-game)
//   3. CONFIGURED app-mount value /app0/eboot.bin — accepted ONLY if it
//      names an existing regular file (ValidateNativeProgramContext enforces
//      exactly this through the composition's own libc++).
// The kve_path 8-byte PREFIX is always logged (discovery evidence for the
// true mount name). Source label: 1 kernel-observed, 2 configured-existing,
// 0 none (caller then fails honestly).
static int bridge_executable_path(void *ctx, char *out, size_t capacity)
{
    if (out == nullptr || capacity == 0) return -1;
    const char *configured = "/app0/eboot.bin";
    const char *value = nullptr;
    uint32_t source = 0;
    if (g_observed_exe_path_valid) { value = g_observed_exe_path; source = 1; }
    const lmf::LiveModuleOsOps &base =
        *static_cast<const lmf::LiveModuleOsOps *>(ctx);
    char probe[8] = {0};  // r13 (review P3-3): defined even if the base op
                          // violates its contract (return 0 without writing)
    if (value == nullptr && base.executable_path != nullptr &&
        base.executable_path(nullptr, probe, sizeof probe) == 0 && probe[0] != '\0') {
        if (base.executable_path(nullptr, out, capacity) == 0 && out[0] != '\0') {
            ::diagnostic_event("bridge_exepath_source", 1u);
            return 0;
        }
    }
    if (value == nullptr) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(configured, ec)) { value = configured; source = 2; }
    }
    if (value == nullptr) {
        // r10 diagnostic: decide the resolution fork empirically. stat three
        // classes of path (title-mount guess; our own module file — known to
        // exist on disk; the activation root). If the own-module stat fails
        // too, std::filesystem is non-functional in-process and every
        // is_regular_file conclusion so far is void; if it succeeds while
        // app0 fails, the mount name is wrong and the r9 kve discovery scan
        // names the truth.
        std::error_code ea, eb, ed;
        ::diagnostic_event("stat_app0",
            std::filesystem::is_regular_file("/app0/eboot.bin", ea) ? 1u : 0u);
        ::diagnostic_event("stat_app0_ec", (uint32_t)ea.value());
        std::error_code eb2;
        ::diagnostic_event("stat_ownmod",
            std::filesystem::is_regular_file(
                "/data/plugins/modbridge_supplier_composition.prx", eb2) ? 1u : 0u);
        ::diagnostic_event("stat_ownmod_ec", (uint32_t)eb2.value());
        std::error_code ed2;
        ::diagnostic_event("stat_rootdir",
            std::filesystem::is_regular_file(
                "/data/modbridge/roots/PPSA33286", ed2) ? 1u : 0u);
        ::diagnostic_event("stat_rootdir_ec", (uint32_t)ed2.value());
        ::diagnostic_event("bridge_exepath_source", 0u);
        return -1;
    }
    size_t i = 0;
    for (; value[i] != '\0' && i + 1 < capacity; ++i) out[i] = value[i];
    if (value[i] != '\0') return -1;
    out[i] = '\0';
    uint32_t sig = 0;
    for (int k = 0; k < 4 && out[k]; ++k) sig |= (uint32_t)(unsigned char)out[k] << (8 * k);
    ::diagnostic_event("bridge_exepath_source", source);
    ::diagnostic_event("bridge_exepath_sig", sig);
    return 0;
}

// r8: PLH owned-memory pre-registration (launch-6 predicted gate 2.6: the
// scan source builds a ReadLease over EVERY readable range the conversion
// derives; unregistered bytes throw). The loader contract (NativeLoader-
// Context.hpp) expects the in-process loader to supply "already-existing
// owned-service registrations" for its retained mappings — this is that
// call. Exec-only text is NOT registered readable — the D2/text_base
// question stays open upstream.
//
// r14 (L1/L2/L5): registration is FROZEN-SNAPSHOT-DERIVED. The registry is
// fed exclusively from the mappings array capture() passed to the
// scan_image_base wrapper — the same array anchor_mappings (and therefore
// ModuleSnapshot::ReadableRanges -> ReadLease ranges) is later built from
// (live_module_facts.cpp:449-457 keeps records containing the chosen base
// from this exact array; lmf_to_native_loader_facts.cpp:77-109 derives the
// lease ranges from those). One snapshot, one derivation -> the registered
// set provably covers every page any ReadLease can walk, with no second
// sysctl to race. Records are registered with their true observed kve
// protection mapped to PLH bits, minus W (the service must never authorize
// writes into game memory; no lease needs it): readable-only pages -> R,
// readable+exec -> R|X. Exec-only / no-protection records are never
// registered. Skip-if-covered makes re-registration idempotent (L5); the
// 128-region cap and any impossible quantization overlap refuse loudly
// through the coverage gate (L3) instead of saturating or throwing deep in
// PLH ("Owned regions must not overlap", OwnedMemory.cpp:54).
#include <polyhook2/OwnedMemory.hpp>
static PLH::OwnedMemory::ScopedRegion *g_image_regions[128];
static unsigned g_image_region_count = 0;

// r14: bounded bookkeeping mirror of the live PLH registry ([b,e) pairs in
// ascending order; merged where quantization made them touch). Backs the
// coverage gate, the idempotence check and the bounded refusal reporting
// without extra PLH calls. Capacity matches the ScopedRegion cap exactly.
struct LeaseSpan { uint64_t b, e; };
static LeaseSpan g_lease_spans[128];
static unsigned g_lease_span_count = 0;
static constexpr uint64_t kPage = 0x4000;  // PS5 page size

// True observed protection for the service record: R always (the record is
// only ever created for kve-readable bytes), X preserved when the kernel
// reports exec. W deliberately dropped: ReadLease consults R only
// (OwnedMemory.cpp:126) and mem_copy's W side must never admit game memory.
static PLH::ProtFlag lease_span_plh_prot(uint32_t kve_protection) noexcept
{
    return (kve_protection & 4u) ? (PLH::ProtFlag)(PLH::R | PLH::X) : PLH::R;
}

// Containing registry span for [addr, addr+len), else nullptr. Pure.
// NOTE the addr < e guard: without it, e - addr underflows for addresses
// past a span and len <= (huge) false-matches — the exact class of bug
// PLH::extent()/covering() guard against (OwnedMemory.cpp:36-48).
static const LeaseSpan *bridge_lease_span_containing(uint64_t addr,
                                                     uint64_t len) noexcept
{
    for (unsigned i = 0; i < g_lease_span_count; ++i) {
        if (addr >= g_lease_spans[i].b && addr < g_lease_spans[i].e &&
            len <= g_lease_spans[i].e - addr)
            return &g_lease_spans[i];
    }
    return nullptr;
}

// Bounded refusal report through the EXISTING diagnostic channel: what was
// requested, and the nearest registered interval (0,0 when the registry is
// empty). Tags follow the lo/high word precedent (mfa_activated_low/high).
static void bridge_report_lease_refusal(const char *what, uint64_t addr,
                                        uint64_t len) noexcept
{
    ::diagnostic_event(what, 1u);
    ::diagnostic_event("bridge_lease_ref_lo", (uint32_t)(addr & 0xffffffffu));
    ::diagnostic_event("bridge_lease_ref_hi", (uint32_t)(addr >> 32));
    ::diagnostic_event("bridge_lease_ref_len", (uint32_t)len);
    const LeaseSpan *near = nullptr;  // spans ascend: first span ending after
    for (unsigned i = 0; i < g_lease_span_count; ++i) {  // the request, else last
        if (g_lease_spans[i].e > addr) { near = &g_lease_spans[i]; break; }
    }
    if (near == nullptr && g_lease_span_count > 0)
        near = &g_lease_spans[g_lease_span_count - 1];
    if (near != nullptr) {
        ::diagnostic_event("bridge_lease_near_lo", (uint32_t)(near->b & 0xffffffffu));
        ::diagnostic_event("bridge_lease_near_hi", (uint32_t)(near->b >> 32));
        ::diagnostic_event("bridge_lease_near_len", (uint32_t)(near->e - near->b));
    }
}

// L2/L5: quantize [b_raw, e_raw) outward to PS5 pages and register with true
// protection. Returns false ONLY on cap saturation (the L3 gate turns that
// into a loud pre-lease refusal when it bites coverage); already-covered
// and empty spans return true.
static bool bridge_register_span(uint64_t b_raw, uint64_t e_raw,
                                 uint32_t kve_protection) noexcept
{
    if (b_raw >= e_raw) return true;
    // r11 (launch-8 root cause): the lease walks PER PAGE; quantize the
    // registration outward (start down, end up) so every page the lease can
    // walk is covered. Kept verbatim from r13.
    const uint64_t b = b_raw & ~(kPage - 1);
    const uint64_t e = (e_raw + kPage - 1) & ~(kPage - 1);
    // L5 idempotence: a fully covered request is already served; registering
    // it again would throw "Owned regions must not overlap"
    // (OwnedMemory.cpp:53-54).
    if (bridge_lease_span_containing(b, e - b) != nullptr) return true;
    uint64_t lo = b;
    if (g_lease_span_count > 0) {
        const LeaseSpan &last = g_lease_spans[g_lease_span_count - 1];
        if (lo < last.b) {
            // Out-of-order record against the ascending registry: registering
            // could overlap an existing PLH region (add() throws). Refuse to
            // guess — skip and let the L3 coverage gate fail closed with the
            // gap named if this record ever carries a derivable lease range.
            ::diagnostic_event("bridge_lease_order", 1u);
            return true;
        }
        if (lo < last.e) lo = last.e;  // clip the touching head; [lo,e) then
    }                                  // never overlaps any existing region
    if (lo >= e) return true;          // fully covered by the previous span
    if (g_image_region_count >= 128 || g_lease_span_count >= 128) return false;
    g_image_regions[g_image_region_count] =
        new PLH::OwnedMemory::ScopedRegion(
            reinterpret_cast<void *>(static_cast<uintptr_t>(lo)),
            static_cast<size_t>(e - lo), lease_span_plh_prot(kve_protection));
    ++g_image_region_count;
    g_lease_spans[g_lease_span_count].b = lo;
    g_lease_spans[g_lease_span_count].e = e;
    ++g_lease_span_count;
    return true;
}

// L1: register every readable record of the FROZEN capture snapshot that
// intersects [extent_begin, extent_end). Called from the scan_image_base
// wrapper — the first op capture() drives — so registration, the window
// anchor and the later conversion all reason over the same bytes. Kept
// diagnostics: bridge_lease_reg_spans (readable∩extent count, the cap input)
// and bridge_lease_reg (registered count) — same tags, frozen basis.
static void bridge_register_from_frozen_snapshot(const lmf::OsVmMapping *mappings,
                                                 size_t count) noexcept
{
    const uint64_t extent_begin = kCandidateBase;
    const uint64_t extent_end = kCandidateBase + kImageExtentVaddr;
    unsigned readable_spans = 0;
    for (size_t i = 0; i < count; ++i) {
        if ((mappings[i].protection & kVmProtRead) == 0) continue;
        const uint64_t b_raw = mappings[i].start < extent_begin ? extent_begin : mappings[i].start;
        const uint64_t e_raw = mappings[i].end > extent_end ? extent_end : mappings[i].end;
        if (b_raw >= e_raw) continue;
        ++readable_spans;
        if (!bridge_register_span(b_raw, e_raw, mappings[i].protection)) {
            // Cap refusal is deferred to the coverage gate (L3): keep counting
            // so the tag still reports the true span total, and mark the gap.
            ::diagnostic_event("bridge_lease_cap", 1u);
        }
    }
    ::diagnostic_event("bridge_lease_reg_spans", (uint32_t)readable_spans);
    ::diagnostic_event("bridge_lease_reg", (uint32_t)g_image_region_count);
}


// ---- launch10-c spanpub (worker-fname-heap-runtime-bounds) -----------------
// Bounded storage + accessors for the seed-admission span table. The pure
// filter is rc_seed::publish_readable_spans (SeedValidation.hpp,
// co-located with this file; freestanding, host-tested — the fixtures drive
// the SAME function end-to-end with a realistic walk). This TU adds only the
// snapshot plumbing. <cstdint> is already included above; SeedValidation.hpp
// is the one new include and pulls nothing beyond <cstddef>/<cstdint>/
// <string_view>.
#include "SeedValidation.hpp"

static rc_seed::VmSpanRecord g_spanpub_records[kBridgeVmmapEntries];  // WHOLE frozen snapshot (all records — non-readable records still prove enumeration coverage for the envelope)
static rc_seed::AddressSpan g_published_readable_spans[rc_seed::kMaxPublishedSpans];
static uint64_t g_spanpub_envelope_end = 0;  // max captured record end (truncation honesty)
static uint32_t g_published_readable_span_count = 0;
static uint32_t g_published_readable_span_total = 0;

static void bridge_spanpub_publish(const lmf::OsVmMapping *mappings, size_t count,
                                   uint64_t extent_lo, uint64_t extent_bytes)
{
    g_published_readable_span_count = 0;
    g_published_readable_span_total = 0;
    g_spanpub_envelope_end = 0;
    // Copy the WHOLE frozen snapshot (same-capture rule: this is the array
    // this function's caller just filled with bridge_query_vmentry — no
    // second sysctl). The pure filter skips non-readable records itself but
    // still counts them toward the captured envelope.
    const size_t bounded = count < kBridgeVmmapEntries ? count : kBridgeVmmapEntries;
    for (size_t i = 0; i < bounded; ++i) {
        g_spanpub_records[i].start = mappings[i].start;
        g_spanpub_records[i].end = mappings[i].end;
        g_spanpub_records[i].protection = static_cast<uint32_t>(mappings[i].protection);
    }
    const rc_seed::PublicationResult pub = rc_seed::publish_readable_spans(
        g_spanpub_records, bounded, extent_lo, extent_bytes,
        g_published_readable_spans, rc_seed::kMaxPublishedSpans);
    g_published_readable_span_count = pub.published;
    g_published_readable_span_total = pub.total;
    g_spanpub_envelope_end = pub.envelope_end;
    ::diagnostic_event("bridge_spanpub_count", pub.published);
    ::diagnostic_event("bridge_spanpub_total", pub.total);
    ::diagnostic_event("bridge_spanpub_cap", pub.cap_saturated);
    ::diagnostic_event("bridge_spanpub_env_low", (uint32_t)(pub.envelope_end & 0xffffffffu));
    ::diagnostic_event("bridge_spanpub_env_high", (uint32_t)(pub.envelope_end >> 32));
}

// Seed-admission span table (class (ii), readable non-image spans only —
// v5 excludes image-extent overlaps at PUBLICATION (see SeedValidation.hpp
// publish_readable_spans); image readability is proven by the anchor
// geometry instead). Intra-module contract: populated by
// bridge_spanpub_publish at module_start, consumed later by the
// initializer's seed path on the same single-threaded init discipline as
// every other bridge state.
extern "C" uint32_t bridge_readable_span_count(void)
{
    return g_published_readable_span_count;
}

extern "C" int bridge_readable_span_at(uint32_t index, uint64_t *lo, uint64_t *hi)
{
    if (index >= g_published_readable_span_count || lo == nullptr || hi == nullptr) {
        return 0;
    }
    *lo = g_published_readable_spans[index].lo;
    *hi = g_published_readable_spans[index].hi;
    return 1;
}

// Captured-envelope end (max record end over the WHOLE snapshot): the
// initializer passes it to check_pool_shape so an admission miss at/above
// the cut refuses as 'snapshot truncated' (omitted mapping) instead of a
// false 'unmapped'. Returns 0 and writes nothing when unavailable.
extern "C" int bridge_capture_envelope(uint64_t *envelope_end)
{
    if (envelope_end == nullptr || g_published_readable_span_count == 0) {
        return 0;
    }
    *envelope_end = g_spanpub_envelope_end;
    return 1;
}

// ---- gameid (worker-gameid-unit staged NEXT candidate, 2026-10-05) ---------
// Self-reporting game identity so identity drift can never masquerade as a
// runtime bug (worker-game-version evidence dir + analyzer_v2 GAME-IDENTITY
// law). Emitted once at module_start, AFTER the spanpub block above (the call
// site sits right after supplier->capture(), so spanpub publish has already
// run). Design for /app0 FIRST: the bridge's own r10 probe (stat_app0 /
// stat_app0_ec) is the established /app0 evidence — 19 captured in-game
// sessions recorded stat_app0=0 ec=2 (ENOENT) with stat_ownmod=1, so on this
// target the /app0 stat may honestly fail; this unit then emits the errno and
// NOTHING else — no sha words, no version tag, no gamedata line. Absence is
// the honest PENDING; a fabricated identity would be the one unforgivable
// failure mode (analyzer_v2 hard-stops on ANY mismatch — a wrong-but-plausible
// tag would kill a good boot).
//
// Mechanisms, all NID-routed, NO raw syscalls (process-fatal in-game, 934):
//   stat  — std::filesystem::file_size/last_write_time(path, error_code), the
//           EXISTING route behind stat_app0/std::filesystem::is_regular_file
//           (libc++ -> NID-resolved libc stat; proven live: stat_ownmod=1).
//   read  — fopen/fread/fclose from the frozen import contract, bounded
//           256 KiB chunks (the .bss budget here is 256 KiB against a bridge
//           that already carries a ~38 MiB vmmap array). Budget: 194,241,424
//           bytes (both pinned eboot manifests' exact size) hashed once per
//           process at module_start — expected 0.2-1 s wall on target; the
//           measured value is emitted as gameid_sha_ms so the budget is
//           evidence, not a promise.
//   line  — the runtime-status gamedata row goes through the module's own
//           status-file mechanism shape (mfa_status POSIX open/write, PROVEN
//           in-game by session 934's runtime-status.txt): a second O_APPEND
//           whole-line write to the SAME <mod_root>/runtime-status.txt, taken
//           only AFTER StartActivatedRuntimeWithAck returned success (state 1
//           'activated' is written by the RuntimeAckSink before start()), so
//           the row lands after state 1 and can never be truncated away by
//           the sink's fresh-session open. It deliberately does NOT use
//           mfa_status_open (that O_TRUNCs — it would destroy the session
//           file) and does NOT forge a 'state\t' row (that would mint a seq
//           number owned by the sink's counter).
//
// Emitted tags (consumer: analyzer_v2 GAME-IDENTITY, worker-runbook-v2):
//   game_eboot_sha256_w0..w3      first 16 digest bytes as four BIG-ENDIAN
//                                 u32 words (analyzer reassembles pack(">I")
//                                 and compares against BOTH pinned manifests
//                                 3b6cfad4/afe0a2a5 — first 128 bits).
//   game_content_version_packed   (major<<24)|(minor<<16)|(patch<<8)|0 parsed
//                                 STRICTLY from /app0/sce_sys/pfs-version.dat
//                                 ASCII "AA.BBB.CCC"; pinned 02.003.000 ->
//                                 0x02030000. Any open/format failure: the
//                                 errno/format tag only, NEVER a guess.
//   gameid_eboot_size_low/high    stat size, established _low/_high halves.
//   gameid_eboot_mtime_low/high   raw file_time_type epoch count, same split.
//   gameid_stat_fold              size_lo ^ mtime_lo ^ mtime_hi — one-glance
//                                 drift tag (the "folded sig"), never primary.
//   gameid_stat_ec/open_ec/ver_ec/ver_fmt/sha_err/toobig   honest refusals.
//   gameid_sha_ms/gameid_unavailable/gameid_done           budget + summary.
// Runtime-only pins (NEVER asserted on host): decrypted-eboot
// 3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc ->
// w0..w3 = 3b6cfad4 7dd17fea 17d429bc e80ae374; toplevel-eboot
// afe0a2a5fc9e1c17b215bdbb3de3c80eb0b3e1aef5c794aea491805899030b7e ->
// w0..w3 = afe0a2a5 fc9e1c17 b215bdbb 3de3c80e.
static const char kGameidEbootPath[] = "/app0/eboot.bin";
static const char kGameidVersionPath[] = "/app0/sce_sys/pfs-version.dat";
static constexpr uint64_t kGameidMaxBytes = 1073741824ull;  // 1 GiB runtime cap
static constexpr size_t kGameidChunk = 262144u;             // 256 KiB .bss

// Self-contained SHA-256 (public-domain shape, no external symbols). Host
// tests (worker-gameid-unit/host-test) extract EXACTLY these marker-bounded
// lines from THIS staged file and drive the NIST vectors against them, so the
// tested bytes are the shipped bytes.
// [gameid-sha256-BEGIN]
struct GameidSha256 {
    uint32_t h[8];
    uint64_t bits;        // total message length in bits
    unsigned char buf[64];
    unsigned buflen;
};

static const uint32_t kGameidSha256K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static uint32_t gameid_sha256_ror(uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32u - n));
}

static void gameid_sha256_init(GameidSha256 &c)
{
    static const uint32_t iv[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    for (unsigned i = 0; i < 8u; ++i) c.h[i] = iv[i];
    c.bits = 0;
    c.buflen = 0;
}

static void gameid_sha256_block(GameidSha256 &c, const unsigned char *p)
{
    uint32_t w[64];
    for (unsigned i = 0; i < 16u; ++i) {
        w[i] = (uint32_t)p[4u * i + 0u] << 24 | (uint32_t)p[4u * i + 1u] << 16 |
               (uint32_t)p[4u * i + 2u] << 8 | (uint32_t)p[4u * i + 3u];
    }
    for (unsigned i = 16u; i < 64u; ++i) {
        const uint32_t s0 = gameid_sha256_ror(w[i - 15u], 7) ^
                            gameid_sha256_ror(w[i - 15u], 18) ^ (w[i - 15u] >> 3);
        const uint32_t s1 = gameid_sha256_ror(w[i - 2u], 17) ^
                            gameid_sha256_ror(w[i - 2u], 19) ^ (w[i - 2u] >> 10);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }
    uint32_t a = c.h[0], b = c.h[1], cc = c.h[2], d = c.h[3];
    uint32_t e = c.h[4], f = c.h[5], g = c.h[6], h = c.h[7];
    for (unsigned i = 0; i < 64u; ++i) {
        const uint32_t S1 = gameid_sha256_ror(e, 6) ^ gameid_sha256_ror(e, 11) ^
                            gameid_sha256_ror(e, 25);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t t1 = h + S1 + ch + kGameidSha256K[i] + w[i];
        const uint32_t S0 = gameid_sha256_ror(a, 2) ^ gameid_sha256_ror(a, 13) ^
                            gameid_sha256_ror(a, 22);
        const uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        const uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c.h[0] += a; c.h[1] += b; c.h[2] += cc; c.h[3] += d;
    c.h[4] += e; c.h[5] += f; c.h[6] += g; c.h[7] += h;
}

static void gameid_sha256_update(GameidSha256 &c, const unsigned char *p, size_t n)
{
    c.bits += (uint64_t)n * 8u;
    while (n > 0) {
        if (c.buflen == 0 && n >= 64u) {
            gameid_sha256_block(c, p);
            p += 64u;
            n -= 64u;
        } else {
            unsigned take = (unsigned)(64u - c.buflen);
            if ((size_t)take > n) take = (unsigned)n;
            for (unsigned i = 0; i < take; ++i) c.buf[c.buflen + i] = p[i];
            c.buflen += take;
            p += take;
            n -= take;
            if (c.buflen == 64u) {
                gameid_sha256_block(c, c.buf);
                c.buflen = 0;
            }
        }
    }
}

static void gameid_sha256_final(GameidSha256 &c, unsigned char out[32])
{
    const uint64_t bits = c.bits;
    unsigned char pad[72];  // 0x80 + zeros + 8-byte big-endian bit count
    unsigned padlen = (c.buflen < 56u) ? (56u - c.buflen) : (120u - c.buflen);
    pad[0] = 0x80;
    for (unsigned i = 1u; i < padlen; ++i) pad[i] = 0x00;
    for (unsigned i = 0; i < 8u; ++i)
        pad[padlen + i] = (unsigned char)((bits >> (56u - 8u * i)) & 0xffu);
    gameid_sha256_update(c, pad, (size_t)padlen + 8u);
    for (unsigned i = 0; i < 8u; ++i) {
        out[4u * i + 0u] = (unsigned char)(c.h[i] >> 24);
        out[4u * i + 1u] = (unsigned char)(c.h[i] >> 16);
        out[4u * i + 2u] = (unsigned char)(c.h[i] >> 8);
        out[4u * i + 3u] = (unsigned char)(c.h[i]);
    }
}
// [gameid-sha256-END]

// Bounded appends (no libc printf family — hand-rolled like
// lease_gap_append_hex above; decimal per the mfa_status put_u64 shape).
static void gameid_append(char *out, unsigned cap, unsigned &used, const char *text)
{
    if (text == nullptr) return;
    for (unsigned i = 0; text[i] != '\0' && used + 1u < cap; ++i) out[used++] = text[i];
}
static void gameid_append_dec(char *out, unsigned cap, unsigned &used, uint64_t v)
{
    char tmp[24];
    unsigned n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v > 0 && n < sizeof tmp) {
        tmp[n++] = (char)('0' + (v % 10ull));
        v /= 10ull;
    }
    while (n > 0 && used + 1u < cap) out[used++] = tmp[--n];
}

// Identity state for the gamedata row: empty string = honestly unavailable.
static bool g_gameid_emitted = false;
static char g_gameid_sha16[17];
static char g_gameid_ver[16];
static char g_gameid_size[24];

static unsigned long long gameid_now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0ull;
    return (unsigned long long)ts.tv_sec * 1000ull +
           (unsigned long long)ts.tv_nsec / 1000000ull;
}

// Strict ASCII "AA.BBB.CCC" (exactly pfs-version.dat's 10 bytes) -> packed
// (major<<24)|(minor<<16)|(patch<<8)|0 — the analyzer_v2 decode law. Returns
// false on ANY deviation; the caller then emits the format tag and NO packed
// version (never fabricated).
static bool gameid_parse_pfs_version(const char *b, unsigned n, uint32_t &packed)
{
    unsigned i = 0;
    uint32_t fields[3] = {0, 0, 0};
    for (unsigned f = 0; f < 3; ++f) {
        unsigned digits = 0;
        while (i < n && b[i] >= '0' && b[i] <= '9') {
            fields[f] = fields[f] * 10u + (uint32_t)(b[i] - '0');
            ++digits;
            ++i;
            if (digits > 3) return false;  // law: AA.BBB.CCC — max 3 per field
        }
        if (digits == 0) return false;
        if (f < 2) {
            if (i >= n || b[i] != '.') return false;
            ++i;
        }
    }
    if (i != n) return false;  // no trailing junk/newline tolerated
    packed = (fields[0] << 24) | (fields[1] << 16) | (fields[2] << 8);
    return true;
}

// Zero-padded on-disk rendering ("02.003.000" shape: 2.3.3 digits — the same
// decode law analyzer_v2 uses, %02d.%03d.%03d).
static void gameid_render_version(char *out, unsigned cap, uint32_t packed)
{
    static const char digits[] = "0123456789";
    const uint32_t f0 = (packed >> 24) & 0xffu;
    const uint32_t f1 = (packed >> 16) & 0xffu;
    const uint32_t f2 = (packed >> 8) & 0xffu;
    unsigned used = 0;
    char tmp[16];
    unsigned n = 0;
    // Decode law shape %02d.%03d.%03d — pad to it, never truncate (a 3-digit
    // major prints all three digits, exactly like printf would).
    if (f0 >= 100u) tmp[n++] = digits[(f0 / 100u) % 10u];
    tmp[n++] = digits[(f0 / 10u) % 10u];
    tmp[n++] = digits[f0 % 10u];
    tmp[n++] = '.';
    tmp[n++] = digits[(f1 / 100u) % 10u];
    tmp[n++] = digits[(f1 / 10u) % 10u];
    tmp[n++] = digits[f1 % 10u];
    tmp[n++] = '.';
    tmp[n++] = digits[(f2 / 100u) % 10u];
    tmp[n++] = digits[(f2 / 10u) % 10u];
    tmp[n++] = digits[f2 % 10u];
    tmp[n] = '\0';
    gameid_append(out, cap, used, tmp);
    out[used] = '\0';
}

// Version: /app0/sce_sys/pfs-version.dat, strict-parse or nothing. Called on
// BOTH the stat-success and stat-failure paths — the errno (or the packed
// value) is exactly the /app0-path evidence the next decision needs.
static void gameid_emit_version(void)
{
    std::FILE *vf = std::fopen(kGameidVersionPath, "rb");
    if (vf == nullptr) {
        ::diagnostic_event("gameid_ver_ec", (uint32_t)errno);
        return;
    }
    char vb[16];
    const size_t vn = std::fread(vb, 1u, sizeof vb, vf);
    const bool vf_err = (std::ferror(vf) != 0);
    std::fclose(vf);
    if (vf_err) {
        ::diagnostic_event("gameid_ver_ec", (uint32_t)EIO);
        return;
    }
    uint32_t packed = 0;
    if (gameid_parse_pfs_version(vb, (unsigned)vn, packed)) {
        ::diagnostic_event("game_content_version_packed", packed);
        gameid_render_version(g_gameid_ver, (unsigned)sizeof g_gameid_ver, packed);
    } else {
        ::diagnostic_event("gameid_ver_fmt", 1u);
    }
}

// The identity emission itself. Runs once per process on the module_start
// thread (the entry guard serializes module_start/module_stop).
static void bridge_gameid_emit(void)
{
    if (g_gameid_emitted) return;
    g_gameid_emitted = true;
    g_gameid_sha16[0] = '\0';
    g_gameid_ver[0] = '\0';
    g_gameid_size[0] = '\0';

    // Stat through the EXISTING std::filesystem route (stat_app0 precedent).
    const std::filesystem::path eboot(kGameidEbootPath);
    std::error_code sec;
    const std::uintmax_t size = std::filesystem::file_size(eboot, sec);
    if (sec) {
        ::diagnostic_event("gameid_stat_ec", (uint32_t)sec.value());
        ::diagnostic_event("gameid_unavailable", 1u);
        gameid_emit_version();  // the errno pair is the /app0 path evidence
        return;
    }
    std::error_code mec;
    const auto mtime = std::filesystem::last_write_time(eboot, mec);
    const uint64_t size64 = (uint64_t)size;
    const uint64_t mtime64 = mec ? 0ull
        : (uint64_t)static_cast<int64_t>(mtime.time_since_epoch().count());
    if (mec) ::diagnostic_event("gameid_mtime_ec", (uint32_t)mec.value());
    ::diagnostic_event("gameid_eboot_size_low", (uint32_t)(size64 & 0xffffffffull));
    ::diagnostic_event("gameid_eboot_size_high", (uint32_t)(size64 >> 32));
    ::diagnostic_event("gameid_eboot_mtime_low", (uint32_t)(mtime64 & 0xffffffffull));
    ::diagnostic_event("gameid_eboot_mtime_high", (uint32_t)(mtime64 >> 32));
    ::diagnostic_event("gameid_stat_fold",
        (uint32_t)((uint32_t)size64 ^ (uint32_t)(mtime64 & 0xffffffffull) ^
                   (uint32_t)(mtime64 >> 32)));
    {
        unsigned used = 0;
        gameid_append_dec(g_gameid_size, (unsigned)sizeof g_gameid_size, used, size64);
        g_gameid_size[used] = '\0';
    }

    // Bounded runtime cap — refuse loudly instead of an unbounded module_start.
    if (size64 == 0ull || size64 > kGameidMaxBytes) {
        ::diagnostic_event("gameid_toobig", 1u);
        ::diagnostic_event("gameid_unavailable", 1u);
        return;
    }

    // Streaming sha256 via the frozen-contract libc FILE* route. A read that
    // comes up short of the stat size refuses the words entirely (a partial
    // digest would be worse than absence).
    const unsigned long long t0 = gameid_now_ms();
    GameidSha256 c;
    gameid_sha256_init(c);
    static unsigned char gameid_chunk_buf[kGameidChunk];
    std::FILE *f = std::fopen(kGameidEbootPath, "rb");
    if (f == nullptr) {
        ::diagnostic_event("gameid_open_ec", (uint32_t)errno);
        ::diagnostic_event("gameid_unavailable", 1u);
        return;
    }
    uint64_t hashed = 0;
    bool io_error = false;
    for (;;) {
        const size_t got = std::fread(gameid_chunk_buf, 1u, kGameidChunk, f);
        if (got > 0) {
            gameid_sha256_update(c, gameid_chunk_buf, got);
            hashed += (uint64_t)got;
        }
        if (got < kGameidChunk) {
            if (std::ferror(f) != 0) io_error = true;
            break;
        }
        if (hashed > size64) break;  // grew mid-read: refuse (never pretend)
    }
    std::fclose(f);
    const unsigned long long t1 = gameid_now_ms();
    ::diagnostic_event("gameid_sha_ms", (uint32_t)(t1 - t0));
    if (io_error || hashed != size64) {
        ::diagnostic_event("gameid_sha_err", 1u);
        ::diagnostic_event("gameid_unavailable", 1u);
        return;
    }
    unsigned char digest[32];
    gameid_sha256_final(c, digest);
    for (unsigned w = 0; w < 4u; ++w) {
        const uint32_t word = (uint32_t)digest[4u * w + 0u] << 24 |
                              (uint32_t)digest[4u * w + 1u] << 16 |
                              (uint32_t)digest[4u * w + 2u] << 8 |
                              (uint32_t)digest[4u * w + 3u];
        char tag[32];
        unsigned used = 0;
        gameid_append(tag, (unsigned)sizeof tag, used, "game_eboot_sha256_w");
        gameid_append_dec(tag, (unsigned)sizeof tag, used, w);
        tag[used] = '\0';
        ::diagnostic_event(tag, word);
    }
    {
        unsigned used = 0;
        static const char hexd[] = "0123456789abcdef";
        for (unsigned i = 0; i < 8u; ++i) {  // 16 hex chars = digest bytes 0..7
            g_gameid_sha16[used++] = hexd[digest[i] >> 4];
            g_gameid_sha16[used++] = hexd[digest[i] & 0xfu];
        }
        g_gameid_sha16[used] = '\0';
    }

    // Version: /app0/sce_sys/pfs-version.dat, strict-parse or nothing.
    gameid_emit_version();
    ::diagnostic_event("gameid_done", 1u);
}

// The human-durable runtime-status gamedata row: written straight after the
// StartActivatedRuntimeWithAck success (RuntimeAckSink state 1 'activated' is
// already in the file), through the module's proven POSIX open/write shape
// (mfa_status lineage) with O_APPEND — never mfa_status_open (O_TRUNC would
// destroy the session file), never a forged 'state\t' seq row.
// Format (task law): gamedata\t<sha16>\t<ver-or-'-'>\t<size>\n
static void bridge_gameid_status_line(const char *mod_root)
{
    if (!g_gameid_emitted || g_gameid_sha16[0] == '\0' || g_gameid_size[0] == '\0')
        return;  // no verified identity -> no row (honest absence)
    if (mod_root == nullptr || mod_root[0] == '\0') return;
    char path[512];
    unsigned used = 0;
    gameid_append(path, (unsigned)sizeof path, used, mod_root);
    if (used + 1u >= (unsigned)sizeof path) return;
    if (path[used - 1u] != '/') path[used++] = '/';
    gameid_append(path, (unsigned)sizeof path, used, "runtime-status.txt");
    path[used] = '\0';
    const int fd = open(path, O_WRONLY | O_APPEND | O_NOFOLLOW, 0600);
    if (fd < 0) {
        ::diagnostic_event("gameid_status_ec", (uint32_t)errno);
        return;
    }
    char line[128];
    unsigned n = 0;
    gameid_append(line, (unsigned)sizeof line, n, "gamedata\t");
    gameid_append(line, (unsigned)sizeof line, n, g_gameid_sha16);
    gameid_append(line, (unsigned)sizeof line, n, "\t");
    if (g_gameid_ver[0] != '\0')
        gameid_append(line, (unsigned)sizeof line, n, g_gameid_ver);
    else
        gameid_append(line, (unsigned)sizeof line, n, "-");
    gameid_append(line, (unsigned)sizeof line, n, "\t");
    gameid_append(line, (unsigned)sizeof line, n, g_gameid_size);
    if (n + 1u < (unsigned)sizeof line) line[n++] = '\n';
    line[n] = '\0';
    size_t off = 0;
    while (off < n) {
        const ssize_t w = write(fd, line + off, n - off);
        if (w <= 0) break;
        off += (size_t)w;
    }
    close(fd);
    ::diagnostic_event("gameid_status_line", off == n ? 1u : 2u);
}

// v5: fresh per-address RANGE mapped-verdict probe for addresses the
// KERN_PROC_VMMAP listing did not return (live candidate evidence 2026-10-05:
// Blocks[0] = 0x22C0000000 vs complete-listing envelope 0x1100400000; direct-
// memory omission is the working hypothesis, NOT an established class).
// mincore(addr, len, vec) answers mapped-ness for the WHOLE range in one
// kernel call: ENOMEM if any page in [addr, addr+len) is unmapped, 0 with the
// vec carrying residency bits otherwise. It touches ZERO target bytes and
// proves NOTHING about read permission (mapped!=readable; this process has
// exec-only text) — resolve_listing_gap (SeedValidation.hpp) holds the
// evidence contract; this bridge function only produces the verdict.
//
// PROVENANCE: this is FRESH per-address evidence at probe time, explicitly
// NOT part of the earlier frozen VMMAP capture; the emitted diagnostic tags
// carry 'fresh' in their name so durable evidence never conflates the two.
//
// The retained SDK libc exports no mincore symbol (syscalls.o defines only
// REMOVED (edit 14, session 934 evidence): bridge_kernel_range_verdict and
// its mincore path. The raw `syscall` instruction is FATAL inside the
// sandboxed game process — the kernel kills the process with 0xa002030a
// SYSTEM_ILLEGAL_FUNCTION_CALL (crash window: after slot-3 seed bind, before
// the pool_range_verdict_fresh tag, whose emission required the syscall to
// return; the linked trampoline disassembles correctly, ruling out
// garbage-execution). Standalone-payload successes with raw syscalls ran in
// homebrew/kernel context and do not transfer. In-game, only NID-resolved
// calls are permitted (sysctl, fopen/fwrite — proven live by this module's
// own log device). The listing-gap probe is replaced by the write-copyin
// probe (bridge_kernel_write_copyin_probe, NID-routed fwrite/fopen; see
// worker-copyin-unit): it provides readability AND the read itself.
// The <sys/syscall.h> include stays harmless; no raw syscall remains.

// L3: the pre-lease coverage gate. Recomputes EXACTLY the ranges the
// conversion will lease — records of the same frozen snapshot that contain
// `observed_base`, are kve-readable, and intersect the extent (the adapter's
// derive_readable_ranges filter: lmf_to_native_loader_facts.cpp:81-107,
// fed from anchor_mappings built at live_module_facts.cpp:449-457) — and
// demands each one fully covered by the registry. Returns false and reports
// the gap when coverage fails; an EMPTY derivable set is NOT a gate failure
// (the conversion refuses honestly with the geometry text on its own).
static bool bridge_verify_lease_coverage(const lmf::OsVmMapping *mappings,
                                         size_t count, uint64_t observed_base,
                                         uint64_t &gap_addr,
                                         uint64_t &gap_len) noexcept
{
    const uint64_t extent_begin = kCandidateBase;
    const uint64_t extent_end = kCandidateBase + kImageExtentVaddr;
    for (size_t i = 0; i < count; ++i) {
        if ((mappings[i].protection & kVmProtRead) == 0) continue;
        if (!(mappings[i].start <= observed_base &&
              observed_base < mappings[i].end)) continue;
        const uint64_t b_raw = mappings[i].start < extent_begin ? extent_begin : mappings[i].start;
        const uint64_t e_raw = mappings[i].end > extent_end ? extent_end : mappings[i].end;
        if (b_raw >= e_raw) continue;
        if (bridge_lease_span_containing(b_raw, e_raw - b_raw) == nullptr) {
            gap_addr = b_raw;
            gap_len = e_raw - b_raw;
            return false;
        }
    }
    return true;
}

// L4: owned-coverage gate for the window-anchor byte reads. A span the fresh
// VMMAP reports readable is still refused unless the frozen registry covers
// it — drift between snapshots refuses loudly (fail-closed) instead of
// dereferencing unowned bytes.
static bool bridge_anchor_owned_covered(uint64_t addr, uint32_t len) noexcept
{
    if (bridge_lease_span_containing(addr, len) != nullptr) return true;
    bridge_report_lease_refusal("bridge_anchor_refuse", addr, len);
    return false;
}

// L3 state: set only inside the scan wrapper (capture), consumed once by the
// start function before conversion. Bounded printable detail for the
// existing runtime-status detail channel (copy_detail filters/caps it).
static bool g_lease_gap_reported = false;
static char g_lease_gap_detail[192];

// Bounded lowercase-hex append (no libc printf family: the packaging
// import-contract refuses new undefined imports, so the bridge hand-rolls
// what the entry's log tooling needs).
static void lease_gap_append_hex(char *out, unsigned cap, unsigned &used,
                                 uint64_t v) noexcept
{
    if (used + 2 >= cap) return;
    out[used++] = '0';
    out[used++] = 'x';
    char tmp[16];
    unsigned n = 0;
    do {
        const unsigned d = (unsigned)(v & 0xfu);
        tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v >>= 4;
    } while (v != 0 && n < sizeof tmp);
    while (n != 0 && used + 1 < cap) out[used++] = tmp[--n];
}

// L3: run after the scan result is known (either route). Verifies every
// range the conversion WILL lease (frozen snapshot, records containing the
// winning chosen base, readable, clamped) against the registry; on a gap
// records the refusal for the start function's pre-conversion check.
static void bridge_gate_after_scan(const lmf::OsVmMapping *mappings,
                                   size_t count, uint64_t chosen_base) noexcept
{
    uint64_t gap_addr = 0, gap_len = 0;
    if (bridge_verify_lease_coverage(mappings, count, chosen_base,
                                     gap_addr, gap_len))
        return;
    g_lease_gap_reported = true;
    ::diagnostic_event("bridge_lease_gap", 1u);
    bridge_report_lease_refusal("bridge_lease_gap_req", gap_addr, gap_len);
    char *d = g_lease_gap_detail;
    unsigned used = 0;
    const unsigned cap = (unsigned)sizeof g_lease_gap_detail;
    const char *head = "lease coverage gap: requested [";
    for (unsigned i = 0; head[i] != '\0' && used + 1 < cap; ++i) d[used++] = head[i];
    lease_gap_append_hex(d, cap, used, gap_addr);
    const char *mid = " +0x";
    for (unsigned i = 0; mid[i] != '\0' && used + 1 < cap; ++i) d[used++] = mid[i];
    lease_gap_append_hex(d, cap, used, gap_len);
    const char *tail = ") outside registered readable spans";
    for (unsigned i = 0; tail[i] != '\0' && used + 1 < cap; ++i) d[used++] = tail[i];
    const LeaseSpan *near = nullptr;
    for (unsigned i = 0; i < g_lease_span_count; ++i) {
        if (g_lease_spans[i].b >= gap_addr + gap_len &&
            (near == nullptr || g_lease_spans[i].b < near->b))
            near = &g_lease_spans[i];
    }
    if (near != nullptr) {
        const char *mid2 = "; nearest registered [";
        for (unsigned i = 0; mid2[i] != '\0' && used + 1 < cap; ++i) d[used++] = mid2[i];
        lease_gap_append_hex(d, cap, used, near->b);
        const char *mid3 = "..";
        for (unsigned i = 0; mid3[i] != '\0' && used + 1 < cap; ++i) d[used++] = mid3[i];
        lease_gap_append_hex(d, cap, used, near->e);
        if (used + 1 < cap) d[used++] = ')';
    }
    d[used] = '\0';
}


extern "C" int modbridge_runtime_stop(void)
{
    auto &slot = owner_slot();
    if (!slot)
    {
        return 0;  // nothing started; nothing to join
    }
    bool stopped = false;
    try
    {
        // Loader control thread (module_stop), never the init thread.
        // Emits at most one 'stopped' ack line, then destroys the owner.
        stopped = modbridge::native::StopRuntimeWithAck(slot);
    }
    catch (...)
    {
        slot.reset();
        return MRS_ERR_STOP_EXN;
    }
    return stopped ? 0 : 1;
}

extern "C" int modbridge_runtime_start_with_activation(const char *module_file_path,
                                                       const char *mod_root,
                                                       const char *title_id,
                                                       struct mfa_activation *activation,
                                                       char *detail,
                                                       unsigned detail_capacity)
{
    auto &slot = owner_slot();
    // r13 (review P2-1, carry-over entry-review P2-2): adopt the activation
    // unconditionally and FIRST, before any early return, so the ownership
    // contract in runtime_start_bridge.h ("OWNERSHIP IS ALWAYS TRANSFERRED on
    // non-NULL input") holds on EVERY exit path including these defensive
    // refusals — a future caller trusting the header cannot leak. The
    // ActivatedMods deleter (ActivationDeleter -> mfa_release) runs on EVERY
    // exit path below, so ownership always lands in exactly one place and
    // nothing leaks on failure. A null activation (not reachable from the C
    // entry, which only calls after a successful mfa_activate) is consumed
    // and refused by the prepare call's own activation validation.
    modbridge::native::ActivatedMods mods{activation};
    if (module_file_path == nullptr || mod_root == nullptr || title_id == nullptr)
    {
        copy_detail(detail, detail_capacity, "bad arguments");
        return MRS_ERR_INTERNAL;
    }
    if (slot)
    {
        copy_detail(detail, detail_capacity, "runtime already started");
        return MRS_ERR_INTERNAL;
    }

    // ---- Stage 1: live module facts -> NativeLoaderFacts (noexcept capture,
    // conversion wrapper records refusals instead of throwing).
    try
    {
        // r8: register the image's readable ranges BEFORE conversion — the
        // scan source's ReadLease (inside MakeNativeProgramContext) requires
        // every readable byte pre-registered.
        // r14 (L1): that registration moved INTO the scan_image_base wrapper
        // below, so it is derived from the SAME frozen capture snapshot the
        // conversion leases from — the pre-capture fresh sysctl (r13
        // bridge_register_readable_ranges) is gone; it was the drift class
        // behind launch-8's "Owned read range includes unregistered bytes".
        lmf::LiveModuleOsOps base_ops = lmf::make_sce_live_module_os_ops(nullptr);
        // r3: wrap scan_image_base so the header scan keeps first shot; when
        // it cannot anchor (exec-only eboot header on this target), the
        // validated data-window pair anchors the image instead.
        lmf::LiveModuleOsOps ops = base_ops;
        ops.context = &base_ops;  // SCE ops ignore ctx; our wrapper reads it
        ops.query_vmentry = bridge_query_vmentry;  // r4: enlarged buffer
        // r7: serve the kernel-observed kve_path for the anchored image when
        // the sysctl KERN_PROC_PATHNAME route is empty (observed ENOENT
        // in-game); fall through to the base op otherwise.
        // r8 (user correction): the observed 144-byte VMMAP records leave only
        // 8 bytes after kve_path offset 136 — a complete path cannot fit, so
        // the kernel-observed route stays honest (NUL-in-record required) and
        // will simply not validate for these records. The fallback is an
        // EXPLICITLY CONFIGURED value (PS5-standard app mount for this title),
        // labeled as configured in diagnostics and NEVER presented as a
        // kernel-observed path. The consumer uses this only for directory/
        // configuration-root derivation; nothing depends on file existence.
        ops.executable_path = bridge_executable_path;
        ops.scan_image_base =
            [](void *ctx, const lmf::OsVmMapping *mappings, size_t count,
               lmf::VmScanResult *out) -> int {
              if (out == nullptr || mappings == nullptr) return -1;
              // r14 (L1): FIRST op of capture(), holding the FROZEN snapshot
              // the whole chain will reason over. Register every readable
              // record intersecting the anchored extent from THESE bytes —
              // never a fresh sysctl (the r13 pre-capture call is retired).
              bridge_register_from_frozen_snapshot(mappings, count);
              // spanpub (launch10 merged): publish the SAME frozen snapshot
              // for the seed-admission path (heap-inclusive, envelope-honest)
              // — same array, same moment: the same-capture rule holds.
              bridge_spanpub_publish(mappings, count, kCandidateBase, kImageExtentVaddr);
              const lmf::LiveModuleOsOps &base =
                  *static_cast<const lmf::LiveModuleOsOps *>(ctx);
              // First: the original header scan (unchanged behavior).
              if (base.scan_image_base != nullptr &&
                  base.scan_image_base(nullptr, mappings, count, out) == 0 &&
                  out->chosen_index >= 0 && out->chosen_base != 0) {
                  bridge_gate_after_scan(mappings, count, out->chosen_base);
                  return 0;
              }
              // Fallback: data-window anchor (validated windows, readable
              // spans only, BOTH windows must verify at the candidate base).
              if (!window_anchor_verify()) return -1;
              lmf::VmScanResult r{};
              r.chosen_index = 0;
              r.chosen_base = kCandidateBase;
              r.chosen_extent = kImageExtentVaddr;
              r.candidate_count = 1;
              r.truncated = 0;
              *out = r;
              bridge_gate_after_scan(mappings, count, r.chosen_base);
              return 0;
            };
        auto supplier = std::make_shared<lmf::RetainedLiveModuleFactsSupplier>(
            ops, prospero_live_firmware_profile());
        supplier->capture();  // VMMAP-first; facts().status records the outcome
        // gameid (worker-gameid-unit): the spanpub block above has published
        // the frozen snapshot; identity self-report is NEXT, once, honestly
        // (stat -> streaming sha256 -> version; on any failure the errno and
        // NOTHING else). This is before the lease-gap check on purpose: a
        // refusing boot still leaves its identity evidence behind.
        bridge_gameid_emit();
        // r14 (L3): post-capture coverage gate. Runs BEFORE conversion so a
        // gap refuses here, naming the gap and the nearest registered span,
        // instead of surfacing as a deep PLH throw inside
        // MakeNativeProgramContext (launch-8's code=2). The flag is only ever
        // set inside the scan wrapper during capture(); an empty derivable
        // set is left to the conversion's own honest geometry refusal.
        if (g_lease_gap_reported)
        {
            copy_detail(detail, detail_capacity, g_lease_gap_detail);
            return MRS_ERR_CONVERSION;
        }

        // Owning conversion overload: ModuleInput.lifetime aliases the
        // supplier (and any retained loader reference) into the facts.
        const std::shared_ptr<const lmf::RetainedLiveModuleFactsSupplier> retained = supplier;
        RC::Platform::LmfConversionOptions options{};
        // In-game primary identity route (checkpoint zcode_ingame_v1_results):
        // VMMAP span + in-memory ELF base scan. Explicit opt-in, default is
        // refusal; without it a VmScan-identity snapshot never converts.
        options.allow_vm_scan_base_as_module_image_base = true;
        // Resolution B (configured roots): the observed executable path is
        // empty on this target. Converting with it EMPTY (never fabricated)
        // behind this explicit opt-in; ValidateNativeProgramContext then
        // requires configured_game_directory, set on the facts below.
        options.allow_configured_roots_for_missing_executable_path = true;

        RC::Platform::LmfConversionResult converted =  // r12: non-const — configured roots attach after conversion
            RC::Platform::TryMakeNativeLoaderFactsFromSupplier(
                retained, std::filesystem::path(module_file_path), options);
        if (!converted)
        {
            // r3 visibility: record WHICH identity route failed so the next
            // log read is unambiguous (vmentry ok? scan ran? link-map rc?).
            const lmf::LiveModuleFactsSnapshot &cap = supplier->facts();
            ::diagnostic_event("lmf_vmentry_cap", (uint32_t)cap.capabilities.vmentry);
            ::diagnostic_event("lmf_vm_scan_cap", (uint32_t)cap.capabilities.vm_scan);
            ::diagnostic_event("lmf_linkmap_cap", (uint32_t)cap.capabilities.link_map_walk);
            ::diagnostic_event("lmf_provider_code", (uint32_t)cap.provider_code);
            ::diagnostic_event("lmf_identity_source", (uint32_t)cap.identity_source);
            // worker-facts-geometry-fix (controller directive, launch-9):
            // conversion-side geometry inputs through the EXISTING diagnostic
            // channel. The per-refusal PREDICATE itself is already durable:
            // each adapter refusal carries a unique static error_message that
            // copy_detail relays verbatim into the r4 UE4SS mirror line and
            // the runtime-status detail string. These four events name the
            // INPUTS that predicate saw, so a zero-interval refusal is
            // readable as "extent [base,end) vs N anchor spans" without any
            // new sink. On a refusal the extent inputs are the snapshot's
            // VmScan facts (the same numbers the window-anchor wrapper fed);
            // the 64-bit halves follow the entry's established _low/_high
            // event convention.
            ::diagnostic_event("lmf_extent_base_low",
                               (uint32_t)(cap.vm_scan.chosen_base & 0xffffffffu));
            ::diagnostic_event("lmf_extent_base_high",
                               (uint32_t)(cap.vm_scan.chosen_base >> 32));
            ::diagnostic_event("lmf_extent_end_low",
                               (uint32_t)((cap.vm_scan.chosen_base + cap.vm_scan.chosen_extent) & 0xffffffffu));
            ::diagnostic_event("lmf_extent_end_high",
                               (uint32_t)((cap.vm_scan.chosen_base + cap.vm_scan.chosen_extent) >> 32));
            ::diagnostic_event("lmf_anchor_count", (uint32_t)cap.anchor_mapping_count);
            ::diagnostic_event("lmf_intervals_found", 0u);
            copy_detail(detail, detail_capacity, converted.error_message);
            return MRS_ERR_CONVERSION;
        }

        // Resolution B: attach the EXPLICIT CONFIGURED ROOTS. mod_root is the
        // entry's MODBRIDGE_MOD_ROOT; the game directory is the configured
        // title-mount prefix. Diagnostics label them configured so a log
        // reader can always distinguish "real path served"
        // (bridge_exepath_source=1/2) from "configured roots carried"
        // (source=0 + bridge_cfg_* events).
        converted.facts.configured_game_directory = std::filesystem::path(kConfiguredGameDirectory);
        converted.facts.configured_working_directory = std::filesystem::path(mod_root);
        ::diagnostic_event("bridge_cfg_gamedir", 1u);
        ::diagnostic_event("bridge_cfg_workdir", 1u);
        // worker-facts-geometry-fix: the same geometry inputs on the SUCCESS
        // path, now from the adapter's resolved extent/provenance record, so
        // every launch logs what the intervals were derived from.
        {
            const auto &image =
                converted.facts.modules.images.at(RC::ScanTarget::MainExe);
            ::diagnostic_event("lmf_extent_base_low",
                               (uint32_t)(converted.resolved_image_base & 0xffffffffu));
            ::diagnostic_event("lmf_extent_base_high",
                               (uint32_t)(converted.resolved_image_base >> 32));
            ::diagnostic_event("lmf_extent_end_low",
                               (uint32_t)(converted.resolved_image_end & 0xffffffffu));
            ::diagnostic_event("lmf_extent_end_high",
                               (uint32_t)(converted.resolved_image_end >> 32));
            ::diagnostic_event("lmf_anchor_count",
                               (uint32_t)supplier->facts().anchor_mapping_count);
            ::diagnostic_event("lmf_intervals_found", (uint32_t)image.readable_ranges.size());
        }

        // ---- Stage 2: NativeProgramContext build + validation. Throws on
        // refused/invalid facts or missing regular files (both paths must
        // exist NOW, while /data is still reachable in module_start).
        try
        {
            RC::Platform::NativeProgramContext context =
                RC::Platform::MakeNativeProgramContext(std::move(converted.facts));

            // ---- Stage 3: THE composition chain, exactly as
            // INTEGRATION-CHECKLIST STEP 6 orders it:
            //   InstallDefaultEngineBindings() once,
            //   RuntimeAckConfig born from mod_root (S1/S2/S3),
            //   PrepareActivatedUE4SSRuntime (opens the single RuntimeAckSink
            //     and builds the NativeModSource from the activation),
            //   owner->start() (serialized init thread; returns immediately).
            try
            {
                slot = modbridge::native::StartActivatedRuntimeWithAck(
                    std::move(context), std::move(mods), std::string_view{title_id},
                    mod_root);
                if (!slot)
                {
                    copy_detail(detail, detail_capacity,
                                "StartActivatedRuntimeWithAck returned null owner");
                    return MRS_ERR_START;
                }
                // gameid (worker-gameid-unit): the ack sink wrote state 1
                // 'activated' during prepare, so appending now lands AFTER
                // state 1 and can never collide with the sink's O_TRUNC open.
                // Verified identity only (no sha words -> no row, ever).
                bridge_gameid_status_line(mod_root);
                copy_detail_or_empty(detail, detail_capacity);
                return MRS_OK;
            }
            catch (const std::exception &error)
            {
                copy_detail(detail, detail_capacity, error.what());
                return MRS_ERR_START;
            }
            catch (...)
            {
                copy_detail(detail, detail_capacity, "unknown exception in prepare/start");
                return MRS_ERR_START;
            }
        }
        catch (const std::exception &error)
        {
            copy_detail(detail, detail_capacity, error.what());
            return MRS_ERR_CONTEXT;
        }
        catch (...)
        {
            copy_detail(detail, detail_capacity, "unknown exception building program context");
            return MRS_ERR_CONTEXT;
        }
    }
    catch (const std::exception &error)
    {
        copy_detail(detail, detail_capacity, error.what());
        return MRS_ERR_CONVERSION;
    }
    catch (...)
    {
        copy_detail(detail, detail_capacity, "unknown exception capturing loader facts");
        return MRS_ERR_CONVERSION;
    }
}
