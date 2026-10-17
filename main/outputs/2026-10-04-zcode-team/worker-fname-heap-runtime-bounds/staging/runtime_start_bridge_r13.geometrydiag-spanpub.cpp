// runtime_start_bridge.cpp — the module_start -> composed-runtime wiring.
//
// worker-fname-heap-runtime-bounds SPANPUB STAGED COPY (launch10-c,
// 2026-10-05): worker-facts-geometry-fix/runtime_start_bridge_r13.geometrydiag.cpp
// (sha c3d6c4de…) + ONLY the published-span additions for the FName-pool
// runtime-bounds admission:
//   * bridge_spanpub_publish(): filters the FROZEN walk-results snapshot
//     (the SAME mappings[] the lease registration consumed — never
//     g_image_regions, which is image-extent-clamped by design) through the
//     host-tested pure filter rc_seed::publish_nonimage_readable_spans
//     (SeedValidation.hpp, co-located): prot&READ, clipped to the outside of
//     [base, base+0xbc602c8), remainder >= one FName block capacity
//     (0x20000 = FNameBlockOffsets * Stride), top-16 by size, published
//     ascending by address. Also publishes the CAPTURED ENVELOPE (max record
//     end over the whole snapshot): the kernel enumeration is address-
//     ordered, so a truncated capture is a prefix — the consumer refuses an
//     admission miss at/above the envelope as 'snapshot truncated' (omitted
//     mapping) instead of a false 'unmapped'. Bridge capture capacity is
//     16,384 records, static bss (r4; launch-2 ENOMEM evidence; launch-9
//     observed vm_total 10,019 -> complete with margin).
//   * extern "C" bridge_readable_span_count() / bridge_readable_span_at()
//     accessors consumed by the initializer TU in the SAME PRX (intra-module
//     link: no new PRX imports or exports).
//   * diagnostics bridge_spanpub_count / bridge_spanpub_total /
//     bridge_spanpub_cap through the existing channel.
// Conversion behavior, the PLH lease registration set, and the geometrydiag
// events are byte-identical to the geometrydiag base; the only touches inside
// bridge_register_readable_ranges are ONE publish call after the existing
// loop and ONE diagnostic line on the walk-failure early return (see
// DIFF.md hunks for the r14+diag re-application anchors).
//
// worker-facts-geometry-fix STAGED COPY (2026-10-05): canonical
// worker-module-start-wiring/runtime_start_bridge_r13.cpp (conversion
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
#include <sys/user.h>
#include <unistd.h>

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
static constexpr size_t kBridgeVmmapEntries = 16384;
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
// call. Each readable span intersecting the anchored image extent registers
// with its true observed protection (R). Regions are static and never
// destroyed: process-lifetime synchronous ownership. Exec-only text is NOT
// registered readable — the D2/text_base question stays open upstream.
#include <polyhook2/OwnedMemory.hpp>
static PLH::OwnedMemory::ScopedRegion *g_image_regions[128];
static unsigned g_image_region_count = 0;

// spanpub (launch10-c) forward declaration — defined below the registration
// function with the rest of the spanpub block (table + accessors).
static void bridge_spanpub_publish(const lmf::OsVmMapping *mappings, size_t count,
                                   uint64_t extent_lo, uint64_t extent_bytes);

static void bridge_register_readable_ranges()
{
    static lmf::OsVmMapping mappings[kBridgeVmmapEntries];
    size_t total = 0;
    if (bridge_query_vmentry(nullptr, mappings, kBridgeVmmapEntries, &total) != 0) {
        ::diagnostic_event("bridge_lease_reg", 0xffffffffu);
        ::diagnostic_event("bridge_spanpub_count", 0);  // spanpub: loud absence — table stays empty
        return;
    }
    const size_t count = total < kBridgeVmmapEntries ? total : kBridgeVmmapEntries;
    const uint64_t extent_begin = kCandidateBase;
    const uint64_t extent_end = kCandidateBase + kImageExtentVaddr;
    constexpr uint64_t kPage = 0x4000;  // PS5 page size
    unsigned registered = 0;
    // r13 (review P3-1): count EVERY readable span intersecting the extent,
    // not only the registered ones, so the 128-region cap can never saturate
    // silently (spans > 128 with reg == 128 reads unambiguously as "capped";
    // launch-7 evidence predicts spans == 6 on this target).
    unsigned readable_spans = 0;  // readable spans inside the extent (cap input)
    for (size_t i = 0; i < count; ++i) {
        if ((mappings[i].protection & kVmProtRead) == 0) continue;
        const uint64_t b_raw = mappings[i].start < extent_begin ? extent_begin : mappings[i].start;
        const uint64_t e_raw = mappings[i].end > extent_end ? extent_end : mappings[i].end;
        if (b_raw >= e_raw) continue;
        ++readable_spans;
        if (registered >= 128) continue;  // cap: keep counting, stop registering
        // r11 (launch-8 root cause): the lease walks PER PAGE; the extent
        // clamp 0xfc602c8 is NOT page-aligned, so the previous exact-end
        // registration left the final partial page unregistered and the lease
        // threw "Owned read range includes unregistered bytes". Quantize the
        // registration outward (start down, end up) so every page the lease
        // can walk is covered.
        const uint64_t b = b_raw & ~(kPage - 1);
        const uint64_t e = (e_raw + kPage - 1) & ~(kPage - 1);
        g_image_regions[registered++] = new PLH::OwnedMemory::ScopedRegion(
            reinterpret_cast<void *>(static_cast<uintptr_t>(b)),
            static_cast<size_t>(e - b), PLH::R);
    }
    g_image_region_count = registered;
    ::diagnostic_event("bridge_lease_reg_spans", (uint32_t)readable_spans);
    ::diagnostic_event("bridge_lease_reg", (uint32_t)registered);
    // spanpub (launch10-c): publish the SAME frozen walk snapshot for the
    // seed-admission path; the initializer TU consumes it via the extern "C"
    // accessors below. Extent inputs are the file-vaddr pair already used
    // above; the filter clips to the OUTSIDE of that extent.
    bridge_spanpub_publish(mappings, count, extent_begin, kImageExtentVaddr);
}

// ---- launch10-c spanpub (worker-fname-heap-runtime-bounds) -----------------
// Bounded storage + accessors for the seed-admission span table. The pure
// filter is rc_seed::publish_nonimage_readable_spans (SeedValidation.hpp,
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
    const rc_seed::PublicationResult pub = rc_seed::publish_nonimage_readable_spans(
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

// Seed-admission span table (class (ii), non-image readable spans only —
// image memory is refused by the extent check on the consumer side). Intra-
// module contract: populated by bridge_register_readable_ranges at
// module_start, consumed later by the initializer's seed path on the same
// single-threaded init discipline as every other bridge state.
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
        bridge_register_readable_ranges();
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
              const lmf::LiveModuleOsOps &base =
                  *static_cast<const lmf::LiveModuleOsOps *>(ctx);
              // First: the original header scan (unchanged behavior).
              if (base.scan_image_base != nullptr &&
                  base.scan_image_base(nullptr, mappings, count, out) == 0 &&
                  out->chosen_index >= 0 && out->chosen_base != 0)
                  return 0;
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
              return 0;
            };
        auto supplier = std::make_shared<lmf::RetainedLiveModuleFactsSupplier>(
            ops, prospero_live_firmware_profile());
        supplier->capture();  // VMMAP-first; facts().status records the outcome

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
